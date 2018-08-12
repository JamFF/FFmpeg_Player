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

// 使用libyuv，将YUV转换RGB
#include <libyuv.h>

#define LOG_I(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO,"JamFF",FORMAT,##__VA_ARGS__);
#define LOG_E(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"JamFF",FORMAT,##__VA_ARGS__);

// nb_streams，媒体文件中存在，音频流，视频流，字幕，这里不考虑字幕，所以设置为2
#define MAX_STREAM 2

// JavaVM 代表的是Java虚拟机，所有的工作都是从JavaVM开始
// 可以通过JavaVM获取到每个线程关联的JNIEnv

// 如何获取JavaVM？
// 1.在JNI_OnLoad函数中获取// 2.2以后版本
// 2.(*env)->GetJavaVM(env,&javaVM);// 兼容各个版本

JavaVM *javaVM;
jobject player_global;
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
              video_stream_idx);
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
    // 一般情况视频是0，音频是1
    player->input_codec_ctx[stream_index] = codec_ctx;
    return 0;
}

/**
 * 解码视频
 */
int decode_video(struct Player *player, AVPacket *packet) {

    int got_picture, ret;

    // AVFrame，像素数据（解码数据），用于存储解码后的像素数据(YUV)
    // 内存分配
    AVFrame *pFrame = av_frame_alloc();// 实际就是YUV420P
    AVFrame *pRGBFrame = av_frame_alloc();// RGB，用于渲染

    if (pRGBFrame == NULL || pFrame == NULL) {
        LOG_E("Could not allocate video frame");
        return -1;
    }

    // 绘制时的缓冲区
    ANativeWindow_Buffer outBuffer;

    AVCodecContext *pCodecCtx = player->input_codec_ctx[player->video_stream_index];

    // 8.解码一帧视频压缩数据，得到视频像素数据，AVPacket->AVFrame
    ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
    if (ret < 0) {
        LOG_E("解码错误");
        return -1;
    }

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

        // 2、设置缓冲区的属性（宽、高、像素格式），像素格式要和SurfaceView的像素格式一直
        ANativeWindow_setBuffersGeometry(player->nativeWindow,
                                         pCodecCtx->width, pCodecCtx->height,
                                         WINDOW_FORMAT_RGBA_8888);

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
    av_frame_free(&pFrame);
    av_frame_free(&pRGBFrame);

    return 0;
}

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

/**
 * 解码音视频，子线程函数
 */
void *decode_data(void *arg) {

    struct Player *player = (struct Player *) arg;
    AVFormatContext *pFormatCtx = player->input_format_ctx;

    // 准备读取
    // AVPacket，编码数据，用于存储一帧一帧的压缩数据（H264）
    // 缓冲区，开辟空间
    AVPacket *packet = av_malloc(sizeof(AVPacket));

    int frame_count = 0;

    flag = 1;

    // 7.一帧一帧的读取压缩视频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0 && flag) {
        if (packet->stream_index == player->video_stream_index) {
            // 只要视频压缩数据（根据流的索引位置判断）
            LOG_I("解码%d帧", ++frame_count);
            decode_video(player, packet);
        }
        // 释放资源
        av_packet_unref(packet);// av_packet_unref代替过时的av_free_packet
    }

    // 关闭解码器
    //avcodec_close(pCodecCtx);

    return NULL;
}


void *decode_data2(void *arg) {

    // 每个线程都有独立的JNIEnv
    JNIEnv *env;
    // 通过JavaVM关联当前线程，获取当前线程的JNIEnv
    (*javaVM)->AttachCurrentThread(javaVM, &env, NULL);

    struct Player *player = (struct Player *) arg;
    AVFormatContext *pFormatCtx = player->input_format_ctx;
    AVCodecContext *pCodecCtx = player->input_codec_ctx[player->video_stream_index];

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
                                     pCodecCtx->width, pCodecCtx->height,
                                     WINDOW_FORMAT_RGBA_8888);

    // av_image_get_buffer_size代替过时的avpicture_get_size
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, pCodecCtx->width,
                                            pCodecCtx->height, 1);
    // 缓冲区分配内存
    uint8_t *buffer = av_malloc(numBytes * sizeof(uint8_t));

    // 初始化缓冲区，av_image_fill_arrays替代过时的avpicture_fill
    av_image_fill_arrays(pRGBFrame->data, pRGBFrame->linesize, buffer, AV_PIX_FMT_RGBA,
                         pCodecCtx->width, pCodecCtx->height, 1);

    // 由于解码出来的帧格式不是RGBA的，在渲染之前需要进行格式转换
    struct SwsContext *sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,// 原始画面宽高
                                                pCodecCtx->pix_fmt,// 原始画面像素格式
                                                pCodecCtx->width, pCodecCtx->height,// 目标画面宽高
                                                AV_PIX_FMT_RGBA,// 目标画面像素格式
                                                SWS_BILINEAR,// 算法
                                                NULL, NULL, NULL);

    int frame_count = 0;

    flag = 1;

    // 准备读取
    // AVPacket，编码数据，用于存储一帧一帧的压缩数据（H264）
    // 缓冲区，开辟空间
    AVPacket *packet = av_malloc(sizeof(AVPacket));

    // 7.一帧一帧的读取压缩视频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0 && flag) {
        if (packet->stream_index == player->video_stream_index) {
            // 只要视频压缩数据（根据流的索引位置判断）
            LOG_I("解码%d帧", ++frame_count);
            decode_video2(player, packet, pCodecCtx, pFrame, pRGBFrame, sws_ctx);
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
    avcodec_close(pCodecCtx);

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

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_init(JNIEnv *env, jobject instance) {

    // 创建全局引用
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

    // 创建子线程，解码视频
    pthread_create(&(player->decode_threads[video_stream_index]), NULL, decode_data,
                   (void *) player);
}

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_play(JNIEnv *env, jobject instance, jstring input_,
                                    jobject surface) {
    LOG_I("play");

    const char *input_cstr = (*env)->GetStringUTFChars(env, input_, 0);

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

    // 创建子线程，解码视频
    pthread_create(&(player->decode_threads[video_stream_index]), NULL, decode_data2,
                   (void *) player);

    (*env)->ReleaseStringUTFChars(env, input_, input_cstr);
}

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_stop(JNIEnv *env, jobject instance) {
    flag = 0;
}

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_destroy(JNIEnv *env, jobject instance) {
    // 释放全局引用，主线程创建的全局引用，必须在主线程释放，子线程释放报错
    (*env)->DeleteGlobalRef(env, player_global);
}