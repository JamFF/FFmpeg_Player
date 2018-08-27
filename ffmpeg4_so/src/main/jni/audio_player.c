#include "com_jamff_ffmpeg_MyPlayer.h"
#include <stdlib.h>
#include <stdio.h>
#include <android/log.h>

// 编解码
#include <libavcodec/avcodec.h>
// 封装格式处理
#include <libavformat/avformat.h>
// 重采样
#include <libswresample/swresample.h>

#define LOG_I(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO,"JamFF",FORMAT,##__VA_ARGS__);
#define LOG_E(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"JamFF",FORMAT,##__VA_ARGS__);

#define MAX_AUDIO_FRAME_SIZE 44100 * 2

// 停止的标记位
int flag;

JNIEXPORT jint JNICALL
Java_com_jamff_ffmpeg_MyPlayer_playMusic(JNIEnv *env, jobject instance, jstring input_jstr,
                                         jstring output_jstr) {
    const char *input_cstr = (*env)->GetStringUTFChars(env, input_jstr, 0);
    const char *output_cstr = (*env)->GetStringUTFChars(env, output_jstr, 0);

    // 1.注册所有组件。已废弃，新版本中不需要调用
    // av_register_all();

    // 封装格式上下文，统领全局的结构体，保存了媒体文件封装格式的相关信息
    AVFormatContext *pFormatCtx = avformat_alloc_context();

    // 2.打开输入媒体文件
    int av_error = avformat_open_input(&pFormatCtx, input_cstr, NULL, NULL);// 第三个参数为NULL，表示自动检测输入格式
    if (av_error != 0) {
        LOG_E("error:%d, %s,", av_error, "无法打开输入媒体文件");
        return -1;
    }

    // 3.获取媒体文件流信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOG_E("无法获取媒体文件信息");
        return -1;
    }

    // 音频解码，需要找到音频对应的AVStream所在pFormatCtx->streams的索引位置
    int audio_stream_idx = -1;
    int i;
    // 遍历所有类型的流（音频流、视频流、字幕流）
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        // 根据类型判断，找到音频流
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            // 获取音频流的索引位置
            audio_stream_idx = i;
            break;
        }
    }

    if (audio_stream_idx == -1) {
        LOG_E("找不到音频流");
        return -1;
    }

    // 音频对应的AVStream
    AVStream *stream = pFormatCtx->streams[audio_stream_idx];

    // 只有知道编码方式，才能够根据编码方式去找到解码器
    // 事实上AVCodecParameters包含了大部分解码器相关的信息
    AVCodecParameters *pCodecParam = stream->codecpar;
    // 4.根据编解码器参数中的编码id查找对应的音频解码器
    AVCodec *pCodec = avcodec_find_decoder(pCodecParam->codec_id);
    if (pCodec == NULL) {
        LOG_E("找不到解码器");
        return -1;
    }

    //5. 初始化编解码上下文
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);// 需要使用avcodec_free_context释放
    // 这里是直接从AVCodecParameters复制到AVCodecContext
    avcodec_parameters_to_context(pCodecCtx, pCodecParam);

    // 6.打开解码器
    // 使用AVCodec初始化AVCodecContext
    // 第一个参数：要初始化的编解码器上下文
    // 第二个参数：从编解码器上下文中得到的编解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOG_E("解码器无法打开");
        return -1;
    }

    // 输出音频信息
    LOG_I("多媒体格式：%s", pFormatCtx->iformat->name);
    LOG_I("时长：%f, %f", (pFormatCtx->duration) / 1000000.0,
          stream->duration * av_q2d(stream->time_base));
    LOG_I("解码器的名称：%s", pCodec->name);

    // 准备读取
    // AVPacket，编码数据，用于存储一帧一帧的压缩数据（H264）
    // 缓冲区，开辟空间
    AVPacket *packet = av_malloc(sizeof(AVPacket));
    if (packet == NULL) {
        LOG_E("分配AVPacket内存失败");
        return -1;
    }

    // AVFrame，解码数据，用于存储解码后的数据
    // 内存分配
    AVFrame *pFrame = av_frame_alloc();// 必须使用av_frame_free()释放
    if (pFrame == NULL) {
        LOG_E("分配输入AVFrame内存失败");
        return -1;
    }

    // 创建重采样上下文
    SwrContext *swrCtx = swr_alloc();
    if (swrCtx == NULL) {
        LOG_E("分配SwrContext失败");
    }

    // 重采样设置参数-------------start

    // 输出的声道布局（立体声）
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;

    // 输出采样格式，16bit
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;

    // 输出采样率，44100Hz，在一秒钟内对声音信号采样44100次
    int out_sample_rate = 44100;

    // 输入的声道布局
    uint64_t in_ch_layout = pCodecCtx->channel_layout;
    LOG_I("in_ch_layout: %"
                  PRIu64
                  "", in_ch_layout);
    // 另一种方式根据声道个数获取默认的声道布局（2个声道，默认立体声stereo）
    /*int64_t in_ch_layout = av_get_default_channel_layout(pCodecCtx->channels);
    LOG_I("in_ch_layout: %"
                  PRIu64
                  "", in_ch_layout);*/

    // 通道布局中的通道数，真正的声道数
    int in_channel_nb = av_get_channel_layout_nb_channels((uint64_t) in_ch_layout);
    LOG_I("in_channels: %d", in_channel_nb);

    // 输入的采样格式
    enum AVSampleFormat in_sample_fmt = pCodecCtx->sample_fmt;

    // 输入采样率
    int in_sample_rate = pCodecCtx->sample_rate;

    // 根据需要分配SwrContext并设置或重置公共参数
    swr_alloc_set_opts(swrCtx,
                       out_ch_layout, out_sample_fmt, out_sample_rate,
                       in_ch_layout, in_sample_fmt, in_sample_rate,
                       0, NULL);// 最后两个是日志相关参数

    // 重采样设置参数-------------end

    // 根据设置的参数，初始化重采样上下文
    swr_init(swrCtx);

    // 16bit 44100 PCM 数据，16bit是2个字节
    uint8_t *out_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRAME_SIZE);

    // 输出的声道个数
    int out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);
    LOG_I("out_channel_nb: %d", out_channel_nb);

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
                                                   out_sample_rate,// createAudioTrack的参数
                                                   out_channel_nb);// createAudioTrack的参数

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

    // 输出文件
    // FILE *fp_pcm = fopen(output_cstr, "wb");

    int ret;

    int frame_count = 0;

    flag = 1;

    // 7.一帧一帧的读取压缩音频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0 && flag) {
        // 只要音频压缩数据（根据流的索引位置判断）
        if (packet->stream_index == audio_stream_idx) {
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

                        LOG_I("解码：%d", ++frame_count);
                        // 9.转换音频
                        ret = swr_convert(swrCtx,// 重采样上下文
                                          &out_buffer,// 输出缓冲区
                                          MAX_AUDIO_FRAME_SIZE,// 每通道采样的可用空间量
                                          (const uint8_t **) pFrame->data,// 输入缓冲区
                                          pFrame->nb_samples);// 一个通道中可用的输入采样数量
                        if (ret < 0) {
                            LOG_E("转换时出错 %d", ret);
                        } else {
                            // 获取给定音频参数所需的缓冲区大小
                            int out_buffer_size = av_samples_get_buffer_size(NULL,
                                                                             out_channel_nb,// 输出的声道个数
                                                                             pFrame->nb_samples,// 一个通道中音频采样数量
                                                                             out_sample_fmt,// 输出采样格式16bit
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
                            (*env)->CallIntMethod(env, audio_track, audio_track_write_mid,
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
        }
        // 释放资源
        av_packet_unref(packet);
    }

    // 四、调用AudioTrack.stop()
    (*env)->CallVoidMethod(env, audio_track, audio_track_stop_mid);

    // 五、调用AudioTrack.release()
    (*env)->CallVoidMethod(env, audio_track, audio_track_release_mid);

    // 关闭文件
    // fclose(fp_pcm);

    // 释放AVFrame
    av_frame_free(&pFrame);

    // 释放缓冲区
    av_free(out_buffer);

    // 释放重采样上下文
    swr_free(&swrCtx);

    // 关闭解码器
    avcodec_free_context(&pCodecCtx);

    // 关闭AVFormatContext
    avformat_close_input(&pFormatCtx);

    (*env)->ReleaseStringUTFChars(env, input_jstr, input_cstr);
    (*env)->ReleaseStringUTFChars(env, output_jstr, output_cstr);

    return 0;
}

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_stopMusic(JNIEnv *env, jobject instance) {
    flag = 0;
}