#include "com_jamff_ffmpeg_MyPlayer.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>
// 需要引入native绘制的头文件
#include <android/native_window_jni.h>
#include <android/native_window.h>

// 编解码
#include <libavcodec/avcodec.h>
// 封装格式处理
#include <libavformat/avformat.h>
// 像素处理
#include <libswscale/swscale.h>

// 编解码，使用av_image...代替libavcodec/avcodec.h中过时的avpicture...
#include <libavutil/imgutils.h>

// 重采样
#include <libswresample/swresample.h>

// 使用libyuv，将YUV转换RGB
#include <libyuv.h>

#define LOG_I(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO,"JamFF",FORMAT,##__VA_ARGS__);
#define LOG_E(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"JamFF",FORMAT,##__VA_ARGS__);

// nb_streams，媒体文件中存在，音频流，视频流，字幕，这里不考虑字幕，所以设置为2
#define MAX_STREAM 2

#define MAX_AUDIO_FRAME_SIZE 44100 * 2

// JavaVM 代表的是Java虚拟机，所有的工作都是从JavaVM开始
// 可以通过JavaVM获取到每个线程关联的JNIEnv

// 如何获取JavaVM？
// 1.在JNI_OnLoad函数中获取// 2.2以后版本
// 2.(*env)->GetJavaVM(env,&javaVM);// 兼容各个版本

JavaVM *javaVM;
jobject player_global;
jobject audio_track_global;
jmethodID player_completion_mid;

// 动态库加载时会执行
// Android SDK 2.2之后才有，2.2没有这个函数
JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOG_I("%s", "JNI_OnLoad");
    // 获取JavaVM
    javaVM = vm;
    return JNI_VERSION_1_4;
}

// 停止的标记位
int flag;

struct Player {
    // 封装格式上下文
    AVFormatContext *input_format_ctx;
    // 音频流索引位置
    int video_stream_index;
    // 视频流索引位置
    int audio_stream_index;
    // 解码器上下文数组
    AVCodecContext *input_codec_ctx[MAX_STREAM];
    // 解码线程ID
    pthread_t decode_threads[MAX_STREAM];
    ANativeWindow *nativeWindow;
    // 播放一帧休眠时常
    int sleep;

    // 音频

    SwrContext *swrCtx;
    // 输入的采样格式
    enum AVSampleFormat in_sample_fmt;
    // 输出采样格式，16bit
    enum AVSampleFormat out_sample_fmt;
    // 输入采样率
    int in_sample_rate;
    // 输出采样率
    int out_sample_rate;
    // 输出的声道布局（立体声）
    int out_channel_nb;

    // JNI
    jmethodID audio_track_write_mid;
};

/**
 * 初始化封装格式上下文，获取音频视频流的索引位置
 */
int init_input_format_ctx(struct Player *player, const char *input_cstr) {

    // 1.注册所有组件
    av_register_all();

    // 封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
    AVFormatContext *format_ctx = avformat_alloc_context();

    // 2.打开输入媒体文件
    int av_error = avformat_open_input(&format_ctx, input_cstr, NULL, NULL);
    if (av_error != 0) {
        LOG_E("error:%d Couldn't open file:%s,", av_error, input_cstr);
        return -1;
    }

    // 3.获取媒体文件信息
    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        LOG_E("Couldn't find stream information");
        return -1;
    }

    // 音频，视频解码，需要找到对应的AVStream所在format_ctx->streams的索引位置
    int video_stream_idx = -1;
    int audio_stream_idx = -1;
    int i;
    // 遍历所有类型的流（音频流、视频流、字幕流）
    for (i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
            && video_stream_idx < 0) {
            // 获取视频流的索引位置
            video_stream_idx = i;
        } else if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO
                   && audio_stream_idx < 0) {
            // 获取音频流的索引位置
            audio_stream_idx = i;
        }
    }

    if (video_stream_idx == -1) {
        // 找不到视频流
        LOG_E("Didn't find a video stream");
        return -1;
    } else if (audio_stream_idx == -1) {
        // 找不到音频流
        LOG_E("Didn't find a audio stream");
        return -1;
    } else {
        LOG_I("video_stream_index = %d, audio_stream_index = %d", video_stream_idx,
              audio_stream_idx);
        // 存储到结构体中，方便后续使用
        player->video_stream_index = video_stream_idx;
        player->audio_stream_index = audio_stream_idx;
        player->input_format_ctx = format_ctx;
    }

    return 0;
}

/**
 * 初始化解码器上下文
 */
int init_codec_context(struct Player *player, int stream_index) {

    AVFormatContext *format_ctx = player->input_format_ctx;

    // 音频、视频对应的AVStream
    AVStream *stream = format_ctx->streams[stream_index];

    // 只有知道编码方式，才能够根据编码方式去找到解码器
    // 4.获取音频流、视频流中的编解码器上下文
    AVCodecContext *codec_ctx = stream->codec;

    // 5.根据编解码上下文中的编码id查找对应的解码器
    AVCodec *pCodec = avcodec_find_decoder(codec_ctx->codec_id);
    // 加密或者没有该编码的解码器
    if (pCodec == NULL) {
        LOG_E("Codec not found");
        return -1;
    }
    LOG_I("解码器的名称：%s", pCodec->name);

    // 6.打开解码器
    if (avcodec_open2(codec_ctx, pCodec, NULL) < 0) {
        LOG_E("Could not open codec");
        return -1;
    }
    if (stream_index > MAX_STREAM - 1) {
        LOG_E("stream_index > MAX_STREAM - 1");
        return -1;
    }
    // 一般情况音频是0，视频是1
    player->input_codec_ctx[stream_index] = codec_ctx;
    return 0;
}

/**
 * 解码视频
 */
int decode_video(struct Player *player, AVPacket *packet, AVCodecContext *pCodecCtx,
                 AVFrame *pFrame, AVFrame *pRGBFrame) {

    int got_picture, ret;

    // 8.解码一帧视频压缩数据，得到视频像素数据，AVPacket->AVFrame
    ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
    if (ret < 0) {
        LOG_E("解码错误");
        return -1;
    }

    // 绘制时的缓冲区
    ANativeWindow_Buffer outBuffer;

    // 为0说明解码完成，非0正在解码
    if (got_picture) {
        // 四种获取方式
        /*double cur_time = packet->pts *
                          av_q2d(player->input_format_ctx->streams[player->video_stream_index]->time_base);*/
        // 值相等，AVFrame拷贝自AVPacket的pts
        /*double cur_time = pFrame->pts *
                          av_q2d(player->input_format_ctx->streams[player->video_stream_index]->time_base);*/
        double cur_time = packet->pts * av_q2d(pCodecCtx->time_base) / 1000;
        // 值相等，AVFrame拷贝自AVPacket的pts
        // double cur_time = pFrame->pts * av_q2d(pCodecCtx->time_base) / 1000;
        LOG_I("解码%f秒", cur_time);

        // 9. 绘制
        // 3、lock锁定下一个即将要绘制的Surface
        ANativeWindow_lock(player->nativeWindow, &outBuffer, NULL);

        // 4、读取帧画面放入缓冲区，指定RGB的AVFrame的像素格式、宽高和缓冲区
        avpicture_fill((AVPicture *) pRGBFrame,
                       outBuffer.bits,// 转换RGB的缓冲区，就使用绘制时的缓冲区，即可完成Surface绘制
                       AV_PIX_FMT_RGBA,// 像素格式
                       pCodecCtx->width, pCodecCtx->height);

        // 5、将YUV420P->RGBA_8888
        I420ToARGB(pFrame->data[0], pFrame->linesize[0],// Y
                   pFrame->data[2], pFrame->linesize[2],// V
                   pFrame->data[1], pFrame->linesize[1],// U
                   pRGBFrame->data[0], pRGBFrame->linesize[0],
                   pCodecCtx->width, pCodecCtx->height);

        // 6、unlock绘制
        ANativeWindow_unlockAndPost(player->nativeWindow);

        // FIXME 由于解码需要时间，这样设置播放会偏慢，目前没有解决方式
        usleep((useconds_t) player->sleep);
    }

    return 0;
}

/**
 * 解码视频
 */
int decode_video2(struct Player *player, AVPacket *packet, AVCodecContext *pCodecCtx,
                  AVFrame *pFrame, AVFrame *pRGBFrame, struct SwsContext *sws_ctx) {

    int got_picture, ret;

    // 8.解码一帧视频压缩数据，得到视频像素数据，AVPacket->AVFrame
    ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
    if (ret < 0) {
        LOG_E("解码错误");
        return -1;
    }

    // 绘制时的缓冲区
    ANativeWindow_Buffer outBuffer;

    // 为0说明解码完成，非0正在解码
    if (got_picture) {
        // 四种获取方式
        /*double cur_time = packet->pts *
                          av_q2d(player->input_format_ctx->streams[player->video_stream_index]->time_base);*/
        // 值相等，AVFrame拷贝自AVPacket的pts
        /*double cur_time = pFrame->pts *
                          av_q2d(player->input_format_ctx->streams[player->video_stream_index]->time_base);*/
        double cur_time = packet->pts * av_q2d(pCodecCtx->time_base) / 1000;
        // 值相等，AVFrame拷贝自AVPacket的pts
        // double cur_time = pFrame->pts * av_q2d(pCodecCtx->time_base) / 1000;
        LOG_I("解码%f秒", cur_time);

        // 9. 绘制
        // 3、lock锁定下一个即将要绘制的Surface
        ANativeWindow_lock(player->nativeWindow, &outBuffer, NULL);

        // 4、格式转换
        sws_scale(sws_ctx, (uint8_t const *const *) pFrame->data,
                  pFrame->linesize, 0, pCodecCtx->height,
                  pRGBFrame->data, pRGBFrame->linesize);

        // 获取stride
        uint8_t *dst = outBuffer.bits;
        int dstStride = outBuffer.stride * 4;
        uint8_t *src = pRGBFrame->data[0];
        int srcStride = pRGBFrame->linesize[0];

        // 5、由于window的stride和帧的stride不同，因此需要逐行复制
        int h;
        for (h = 0; h < pCodecCtx->height; h++) {
            memcpy(dst + h * dstStride, src + h * srcStride, (size_t) srcStride);
        }

        // 6、unlock绘制
        ANativeWindow_unlockAndPost(player->nativeWindow);

        // FIXME 由于解码需要时间，这样设置播放会偏慢，目前没有解决方式
        usleep((useconds_t) player->sleep);
    }

    return 0;
}

void jni_audio_prepare(JNIEnv *env, jobject instance, struct Player *player) {
    // JNI-----------------------start

    // 获取MyPlayer的jclass
    jclass player_class = (*env)->GetObjectClass(env, instance);

    // 得到MyPlayer.createAudioTrack()
    jmethodID create_audio_track_mid = (*env)->GetMethodID(env, player_class,// jclass
                                                           "createAudioTrack",// 方法名
                                                           "(II)Landroid/media/AudioTrack;");// 字段签名

    // 一、调用MyPlayer.createAudioTrack()，创建AudioTrack实例
    jobject audio_track = (*env)->CallObjectMethod(env, instance,// jobject
                                                   create_audio_track_mid,// createAudioTrack()
                                                   player->out_sample_rate,// createAudioTrack的参数
                                                   player->out_channel_nb);// createAudioTrack的参数

    // 获取AudioTrack的jclass
    jclass audio_track_class = (*env)->GetObjectClass(env, audio_track);

    // 得到AudioTrack.play()
    jmethodID audio_track_play_mid = (*env)->GetMethodID(env, audio_track_class, "play", "()V");

    // 二、调用AudioTrack.play()
    (*env)->CallVoidMethod(env, audio_track, audio_track_play_mid);

    // 得到AudioTrack.write()
    jmethodID audio_track_write_mid = (*env)->GetMethodID(env, audio_track_class,
                                                          "write", "([BII)I");

    // 得到AudioTrack.stop()
    jmethodID audio_track_stop_mid = (*env)->GetMethodID(env, audio_track_class, "stop", "()V");

    // 得到AudioTrack.release()
    jmethodID audio_track_release_mid = (*env)->GetMethodID(env, audio_track_class,
                                                            "release", "()V");

    // JNI-----------------------end

    // 必须创建全局引用
    audio_track_global = (*env)->NewGlobalRef(env, audio_track);
    player->audio_track_write_mid = audio_track_write_mid;
}

/**
 * 解码音频
 */
int decode_audio(JNIEnv *env, struct Player *player, AVPacket *packet,
                 AVCodecContext *pCodecCtx, AVFrame *pFrame, uint8_t *out_buffer) {

    int ret;

    // 8.解码一帧音频压缩数据，得到音频PCM数据，AVPacket->AVFrame
    ret = avcodec_send_packet(pCodecCtx, packet);
    if (ret < 0) {
        LOG_E("发送数据包到解码器时出错 %d", ret);
        return -1;
    }
    while (ret >= 0) {
        ret = avcodec_receive_frame(pCodecCtx, pFrame);
        switch (ret) {

            case AVERROR(EAGAIN):// 输出是不可用的，必须发送新的输入
                break;
            case AVERROR_EOF:// 已经完全刷新，不会再有输出帧了
                break;
            case AVERROR(EINVAL):// codec打不开，或者是一个encoder
                break;
            case 0:// 成功，返回一个输出帧

                // LOG_I("解码：%d", ++frame_count);
                // 9.转换音频
                ret = swr_convert(player->swrCtx,// 重采样上下文
                                  &out_buffer,// 输出缓冲区
                                  MAX_AUDIO_FRAME_SIZE,// 每通道采样的可用空间量
                                  (const uint8_t **) pFrame->data,// 输入缓冲区
                                  pFrame->nb_samples);// 一个通道中可用的输入采样数量
                if (ret < 0) {
                    LOG_E("转换时出错 %d", ret);
                } else {
                    // 获取给定音频参数所需的缓冲区大小
                    int out_buffer_size = av_samples_get_buffer_size(NULL,
                                                                     player->out_channel_nb,// 输出的声道个数
                                                                     pFrame->nb_samples,// 一个通道中音频采样数量
                                                                     player->out_sample_fmt,// 输出采样格式16bit
                                                                     1);// 缓冲区大小对齐（0 = 默认值，1 = 不对齐）
                    // 10.输出PCM文件
                    //fwrite(out_buffer, 1, (size_t) out_buffer_size, fp_pcm);

                    // ---------------------播放音乐需要的逻辑---------------------

                    // out_buffer缓冲区数据，转成byte数组

                    // 调用AudioTrack.write()时，需要创建jbyteArray
                    jbyteArray audio_sample_array = (*env)->NewByteArray(env,
                                                                         out_buffer_size);

                    // 拷贝数组需要对指针操作
                    jbyte *sample_bytep = (*env)->GetByteArrayElements(env,
                                                                       audio_sample_array,
                                                                       NULL);

                    // out_buffer的数据拷贝到sample_bytep
                    memcpy(sample_bytep,// 目标dest所指的内存地址
                           out_buffer,// 源src所指的内存地址的起始位置
                           (size_t) out_buffer_size);// 拷贝字节的数据的大小

                    // 同步到audio_sample_array，并释放sample_bytep，与GetByteArrayElements对应
                    // 如果不调用，audio_sample_array里面是空的，播放无声，并且会内存泄漏
                    (*env)->ReleaseByteArrayElements(env, audio_sample_array,
                                                     sample_bytep, 0);

                    // 三、调用AudioTrack.write()
                    (*env)->CallIntMethod(env, audio_track_global, player->audio_track_write_mid,
                                          audio_sample_array,// 需要播放的数据数组
                                          0, out_buffer_size);

                    // 释放局部引用，对应NewByteArray
                    (*env)->DeleteLocalRef(env, audio_sample_array);
                }
                break;
            default:// 合法的解码错误
                LOG_E("从解码器接收帧时出错 %d", ret);
                break;
        }
    }
    return 0;
}

/**
 * 解码音视频，子线程函数
 */
void *decode_data(void *arg) {

    // 每个线程都有独立的JNIEnv
    JNIEnv *env;
    // 通过JavaVM关联当前线程，获取当前线程的JNIEnv
    (*javaVM)->AttachCurrentThread(javaVM, &env, NULL);

    struct Player *player = (struct Player *) arg;
    AVFormatContext *pFormatCtx = player->input_format_ctx;
    AVCodecContext *pCodecCtx_video = player->input_codec_ctx[player->video_stream_index];
    AVCodecContext *pCodecCtx_audio = player->input_codec_ctx[player->audio_stream_index];

    // AVFrame，像素数据（解码数据），用于存储解码后的像素数据(YUV)
    // 内存分配
    AVFrame *pFrame = av_frame_alloc();// 实际就是YUV420P
    AVFrame *pRGBFrame = av_frame_alloc();// RGB，用于渲染

    if (pRGBFrame == NULL || pFrame == NULL) {
        LOG_E("Could not allocate video frame");
        goto end;
    }

    // 2、设置缓冲区的属性（宽、高、像素格式），像素格式要和SurfaceView的像素格式一直
    ANativeWindow_setBuffersGeometry(player->nativeWindow,
                                     pCodecCtx_video->width, pCodecCtx_video->height,
                                     WINDOW_FORMAT_RGBA_8888);

    int frame_count_video = 0;
    int frame_count_audio = 0;

    // 16bit 44100 PCM 数据，16bit是2个字节（重采样缓冲区）
    uint8_t *out_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRAME_SIZE);

    flag = 1;

    // 准备读取
    // AVPacket，编码数据，用于存储一帧一帧的压缩数据（H264）
    // 缓冲区，开辟空间
    AVPacket *packet = av_malloc(sizeof(AVPacket));
    if (packet == NULL) {
        LOG_E("分配AVPacket内存失败");
        return NULL;
    }

    // 7.一帧一帧的读取压缩视频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0 && flag) {
        if (packet->stream_index == player->video_stream_index) {
            // 视频压缩数据（根据流的索引位置判断）
            /*LOG_I("解码视频%d帧", ++frame_count_video);
            decode_video(player, packet, pCodecCtx_video, pFrame, pRGBFrame);*/
        } else if (packet->stream_index == player->audio_stream_index) {
            // 音频压缩数据（根据流的索引位置判断）
            LOG_I("解码视频%d帧", ++frame_count_audio);
            decode_audio(env, player, packet, pCodecCtx_audio, pFrame, out_buffer);
        }
        // 释放资源
        av_packet_unref(packet);// av_packet_unref代替过时的av_free_packet
    }

    end:
    // 7、释放资源
    ANativeWindow_release(player->nativeWindow);

    // 释放AVFrame
    av_frame_free(&pFrame);
    av_frame_free(&pRGBFrame);

    // 释放缓冲区
    av_free(out_buffer);

    // 关闭解码器
    avcodec_close(pCodecCtx_video);
    avcodec_close(pCodecCtx_audio);

    // 释放AVFormatContext
    avformat_close_input(&pFormatCtx);

    // 调用MyPlayer.onCompletion()
    (*env)->CallVoidMethod(env, player_global,// jobject
                           player_completion_mid,// onCompletion()
                           0);// onCompletion的参数

    // 取消关联
    (*javaVM)->DetachCurrentThread(javaVM);

    return NULL;
}

/**
 *  解码音视频，子线程函数
 */
void *decode_data2(void *arg) {

    // 每个线程都有独立的JNIEnv
    JNIEnv *env;
    // 通过JavaVM关联当前线程，获取当前线程的JNIEnv
    (*javaVM)->AttachCurrentThread(javaVM, &env, NULL);

    struct Player *player = (struct Player *) arg;
    AVFormatContext *pFormatCtx = player->input_format_ctx;
    AVCodecContext *pCodecCtx_video = player->input_codec_ctx[player->video_stream_index];
    AVCodecContext *pCodecCtx_audio = player->input_codec_ctx[player->audio_stream_index];

    // AVFrame，像素数据（解码数据），用于存储解码后的像素数据(YUV)
    // 内存分配
    AVFrame *pFrame = av_frame_alloc();// 实际就是YUV420P
    AVFrame *pRGBFrame = av_frame_alloc();// RGB，用于渲染

    if (pRGBFrame == NULL || pFrame == NULL) {
        LOG_E("Could not allocate video frame");
        goto end;
    }

    // 2、设置缓冲区的属性（宽、高、像素格式），像素格式要和SurfaceView的像素格式一直
    ANativeWindow_setBuffersGeometry(player->nativeWindow,
                                     pCodecCtx_video->width, pCodecCtx_video->height,
                                     WINDOW_FORMAT_RGBA_8888);

    // av_image_get_buffer_size代替过时的avpicture_get_size
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, pCodecCtx_video->width,
                                            pCodecCtx_video->height, 1);
    // 缓冲区分配内存
    uint8_t *buffer = av_malloc(numBytes * sizeof(uint8_t));

    // 初始化缓冲区，av_image_fill_arrays替代过时的avpicture_fill
    av_image_fill_arrays(pRGBFrame->data, pRGBFrame->linesize, buffer, AV_PIX_FMT_RGBA,
                         pCodecCtx_video->width, pCodecCtx_video->height, 1);

    // 由于解码出来的帧格式不是RGBA的，在渲染之前需要进行格式转换
    struct SwsContext *sws_ctx = sws_getContext(pCodecCtx_video->width,
                                                pCodecCtx_video->height,// 原始画面宽高
                                                pCodecCtx_video->pix_fmt,// 原始画面像素格式
                                                pCodecCtx_video->width,
                                                pCodecCtx_video->height,// 目标画面宽高
                                                AV_PIX_FMT_RGBA,// 目标画面像素格式
                                                SWS_BILINEAR,// 算法
                                                NULL, NULL, NULL);

    int frame_count_video = 0;
    int frame_count_audio = 0;

    // 16bit 44100 PCM 数据，16bit是2个字节（重采样缓冲区）
    uint8_t *out_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRAME_SIZE);

    flag = 1;

    // 准备读取
    // AVPacket，编码数据，用于存储一帧一帧的压缩数据（H264）
    // 缓冲区，开辟空间
    AVPacket *packet = av_malloc(sizeof(AVPacket));

    // 7.一帧一帧的读取压缩视频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0 && flag) {
        if (packet->stream_index == player->video_stream_index) {
            // 只要视频压缩数据（根据流的索引位置判断）
            /*LOG_I("解码%d帧", ++frame_count_video);
            decode_video2(player, packet, pCodecCtx_video, pFrame, pRGBFrame, sws_ctx);*/
        } else if (packet->stream_index == player->audio_stream_index) {
            // 音频压缩数据（根据流的索引位置判断）
            LOG_I("解码视频%d帧", ++frame_count_audio);
            decode_audio(env, player, packet, pCodecCtx_audio, pFrame, out_buffer);
        }
        // 释放资源
        av_packet_unref(packet);// av_packet_unref代替过时的av_free_packet
    }

    // 释放SwsContext
    sws_freeContext(sws_ctx);

    // 释放缓冲区
    av_free(buffer);

    end:
    // 7、释放资源
    ANativeWindow_release(player->nativeWindow);

    // 释放AVFrame
    av_frame_free(&pFrame);
    av_frame_free(&pRGBFrame);

    // 关闭解码器
    avcodec_close(pCodecCtx_video);
    avcodec_close(pCodecCtx_audio);

    // 释放AVFormatContext
    avformat_close_input(&pFormatCtx);

    // 调用MyPlayer.onCompletion()
    (*env)->CallVoidMethod(env, player_global,// jobject
                           player_completion_mid,// onCompletion()
                           0);// onCompletion的参数

    // 取消关联
    (*javaVM)->DetachCurrentThread(javaVM);

    return NULL;
}

/**
 * 解码视频准备工作
 */
void decode_video_prepare(JNIEnv *env, struct Player *player,
                          jobject surface, int video_stream_index) {

    /***********************************打印信息 start***********************************/
    AVCodecContext *pCodecCtx = player->input_codec_ctx[video_stream_index];

    // 获取视频宽高
    int videoWidth = pCodecCtx->width;
    int videoHeight = pCodecCtx->height;

    // 封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
    AVFormatContext *pFormatCtx = player->input_format_ctx;

    // 输出视频信息
    LOG_I("多媒体格式：%s", pFormatCtx->iformat->name);
    // 视频AVStream
    AVStream *stream = pFormatCtx->streams[video_stream_index];
    LOG_I("时长：%f, %f", (pFormatCtx->duration) / 1000000.0,
          stream->duration * av_q2d(stream->time_base));
    LOG_I("视频的宽高：%d, %d", videoWidth, videoHeight);
    // 视频帧率，每秒多少帧
    double frame_rate = av_q2d(stream->avg_frame_rate);
    LOG_I("帧率 = %f", frame_rate);
    /***********************************打印信息 end***********************************/

    // 计算每帧的间隔，单位是微秒
    player->sleep = (int) (1000 * 1000 / frame_rate);
    // 1、获取一个关联Surface的NativeWindow窗体
    player->nativeWindow = ANativeWindow_fromSurface(env, surface);
}

/**
 * 解码音频准备工作
 */
void decode_audio_prepare(struct Player *player, int audio_stream_index) {

    /***********************************打印信息 start***********************************/
    AVCodecContext *pCodecCtx = player->input_codec_ctx[audio_stream_index];

    // 获取视频宽高
    int videoWidth = pCodecCtx->width;
    int videoHeight = pCodecCtx->height;

    // 封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
    AVFormatContext *pFormatCtx = player->input_format_ctx;

    // 输出视频信息
    LOG_I("多媒体格式：%s", pFormatCtx->iformat->name);
    // 视频AVStream
    AVStream *stream = pFormatCtx->streams[audio_stream_index];
    LOG_I("时长：%f, %f", (pFormatCtx->duration) / 1000000.0,
          stream->duration * av_q2d(stream->time_base));
    LOG_I("视频的宽高：%d, %d", videoWidth, videoHeight);
    // 视频帧率，每秒多少帧
    double frame_rate = av_q2d(stream->avg_frame_rate);
    LOG_I("帧率 = %f", frame_rate);
    /***********************************打印信息 end***********************************/

    // 创建重采样上下文
    SwrContext *swrCtx = swr_alloc();
    if (swrCtx == NULL) {
        LOG_E("分配SwrContext失败");
    }

    // 重采样设置参数-------------start

    // 输入的采样格式
    enum AVSampleFormat in_sample_fmt = pCodecCtx->sample_fmt;

    // 输入采样率
    int in_sample_rate = pCodecCtx->sample_rate;

    // 输出采样格式，16bit
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;

    // 输出采样率，44100Hz，在一秒钟内对声音信号采样44100次
    // int out_sample_rate = 44100;// TODO
    int out_sample_rate = in_sample_rate;

    // 输入的声道布局
    uint64_t in_ch_layout = pCodecCtx->channel_layout;
    // 另一种方式根据声道个数获取默认的声道布局（2个声道，默认立体声stereo）
    // int64_t in_ch_layout = av_get_default_channel_layout(pCodecCtx->channels);

    // 输出的声道布局（立体声）
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;

    // 输出的声道个数
    int out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);
    LOG_I("out_channel_nb: %d", out_channel_nb);

    // 根据需要分配SwrContext并设置或重置公共参数
    swr_alloc_set_opts(swrCtx,
                       out_ch_layout, out_sample_fmt, out_sample_rate,
                       in_ch_layout, in_sample_fmt, in_sample_rate,
                       0, NULL);// 最后两个是日志相关参数

    // 重采样设置参数-------------end

    // 根据设置的参数，初始化重采样上下文
    swr_init(swrCtx);

    player->in_sample_fmt = in_sample_fmt;
    player->out_sample_fmt = out_sample_fmt;
    player->in_sample_rate = in_sample_rate;
    player->out_sample_rate = out_sample_rate;
    player->out_channel_nb = out_channel_nb;
    player->swrCtx = swrCtx;
}

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_init(JNIEnv *env, jobject instance) {

    // 必须创建全局引用
    player_global = (*env)->NewGlobalRef(env, instance);

    // 获取MyPlayer的jclass，获取class必须要在主线程
    jclass player_class = (*env)->GetObjectClass(env, player_global);

    // 得到MyPlayer.onCompletion()
    player_completion_mid = (*env)->GetMethodID(env, player_class,// jclass
                                                "onCompletion",// 方法名
                                                "(I)V");// 字段签名
}

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_render(JNIEnv *env, jobject instance, jstring input_jstr,
                                      jobject surface) {
    LOG_I("render");

    const char *input_cstr = (*env)->GetStringUTFChars(env, input_jstr, 0);

    struct Player *player = (struct Player *) malloc(sizeof(struct Player));

    // 初始化封装格式上下文
    if (init_input_format_ctx(player, input_cstr) < 0) {
        return;
    }

    int video_stream_index = player->video_stream_index;
    int audio_stream_index = player->audio_stream_index;

    // 获取视频解码器并打开
    if (init_codec_context(player, video_stream_index) < 0) {
        return;
    }
    // 获取音频解码器并打开
    if (init_codec_context(player, audio_stream_index) < 0) {
        return;
    }

    // 解码视频准备工作
    decode_video_prepare(env, player, surface, video_stream_index);

    // 解码音频准备工作
    decode_audio_prepare(player, audio_stream_index);

    jni_audio_prepare(env, instance, player);

    // TODO 创建子线程，解码视频
    /*pthread_create(&(player->decode_threads[video_stream_index]), NULL, decode_data,
                   (void *) player);*/

    // 创建子线程，解码音频
    pthread_create(&(player->decode_threads[audio_stream_index]), NULL, decode_data,
                   (void *) player);

    (*env)->ReleaseStringUTFChars(env, input_jstr, input_cstr);
}

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_play(JNIEnv *env, jobject instance, jstring input_jstr,
                                    jobject surface) {
    LOG_I("play");

    const char *input_cstr = (*env)->GetStringUTFChars(env, input_jstr, 0);

    struct Player *player = (struct Player *) malloc(sizeof(struct Player));

    // 初始化封装格式上下文
    if (init_input_format_ctx(player, input_cstr) < 0) {
        return;
    }

    int video_stream_index = player->video_stream_index;
    int audio_stream_index = player->audio_stream_index;

    // 获取视频解码器并打开
    if (init_codec_context(player, video_stream_index) < 0) {
        return;
    }
    // 获取音频解码器并打开
    if (init_codec_context(player, audio_stream_index) < 0) {
        return;
    }

    // 解码视频准备工作
    decode_video_prepare(env, player, surface, video_stream_index);

    // 解码音频准备工作
    decode_audio_prepare(player, audio_stream_index);

    jni_audio_prepare(env, instance, player);

    // TODO 创建子线程，解码视频
    /*pthread_create(&(player->decode_threads[video_stream_index]), NULL, decode_data2,
                   (void *) player);*/

    // 创建子线程，解码音频
    pthread_create(&(player->decode_threads[audio_stream_index]), NULL, decode_data2,
                   (void *) player);

    (*env)->ReleaseStringUTFChars(env, input_jstr, input_cstr);
}

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_stop(JNIEnv *env, jobject instance) {
    flag = 0;
}

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_destroy(JNIEnv *env, jobject instance) {
    // 释放全局引用，主线程创建的全局引用，必须在主线程释放，子线程释放报错
    (*env)->DeleteGlobalRef(env, player_global);
    (*env)->DeleteGlobalRef(env, audio_track_global);
}