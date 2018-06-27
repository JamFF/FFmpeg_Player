#include <jni.h>
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

JNIEXPORT jint JNICALL
Java_com_jamff_ffmpeg_DecodeUtils_decodeAudio(JNIEnv *env, jclass type, jstring input_jstr,
                                              jstring output_jstr) {
    const char *input_cstr = (*env)->GetStringUTFChars(env, input_jstr, 0);
    const char *output_cstr = (*env)->GetStringUTFChars(env, output_jstr, 0);

    // 1.注册所有组件
    av_register_all();

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
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
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

    // 只有知道音频的编码方式，才能够根据编码方式去找到解码器
    // 4.获取音频流中的编解码器上下文
    AVCodecContext *pCodecCtx = stream->codec;// 由libavformat分配和释放

    // 5.根据编解码器上下文中的编码id查找对应的音频解码器
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    // 例如加密或者没有该编码的解码器
    if (pCodec == NULL) {
        LOG_E("找不到解码器");
        return -1;
    }

    // 6.打开解码器
    // 使用AVCodec初始化AVCodecContext
    // 第一个参数：要初始化的编解码器上下文
    // 第二个参数：从编解码器上下文中得到的编解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOG_E("解码器无法打开");
        return -1;
    }

    // 输出音频信息
    LOG_I("视频的文件格式：%s", pFormatCtx->iformat->name);
    LOG_I("视频时长：%f, %f", (pFormatCtx->duration) / 1000000.0,
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

    // pFrame->16bit 44100Hz PCM，统一音频采样格式与采样率
    // 采样率是在一秒钟内对声音信号的采样次数
    // TODO

    // 创建重采样上下文
    SwrContext *swrCtx = swr_alloc();
    if (swrCtx == NULL) {
        LOG_E("分配SwrContext失败");
    }

    // 重采样设置参数-------------start

    // 输出的声道布局（立体声）
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;

    // 输出采样格式16bit
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;

    // 输出采样率
    int out_sample_rate = 44100;

    // 输入的声道布局
    uint64_t in_ch_layout = pCodecCtx->channel_layout;
    // 另一种方式根据声道个数获取默认的声道布局（2个声道，默认立体声stereo）
    // int64_t in_ch_layout = av_get_default_channel_layout(pCodecCtx->channels);

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

    // 输出文件
    FILE *fp_pcm = fopen(output_cstr, "wb");

    int got_frame, ret;

    int frame_count = 0;

    // 7.一帧一帧的读取压缩音频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        // 8.解码一帧音频压缩数据，得到音频PCM数据，AVPacket->AVFrame
        ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_frame, packet);
        if (ret < 0) {
            LOG_E("解码错误 %d", ret);
            // FIXME 第一帧会返回-1094995529，AVERROR_INVALIDDATA
            // return -1;
        }

        // 为0说明没有帧可以解压缩，非0正在解码
        if (got_frame > 0) {
            LOG_I("解码：%d", ++frame_count);
            // 9.转换音频
            ret = swr_convert(swrCtx,// 重采样上下文
                              &out_buffer,// 输出缓冲区
                              MAX_AUDIO_FRAME_SIZE,// 每通道采样的可用空间量
                              (const uint8_t **) pFrame->data,// 输入缓冲区
                              pFrame->nb_samples);// 一个通道中可用的输入采样数量
            if (ret < 0) {
                LOG_E("转换时出错");
            } else {
                // 获取给定音频参数所需的缓冲区大小
                int out_buffer_size = av_samples_get_buffer_size(NULL, out_channel_nb,// 输出的声道个数
                                                                 pFrame->nb_samples,// 一个通道中音频采样数量
                                                                 out_sample_fmt,// 输出采样格式16bit
                                                                 1);// 缓冲区大小对齐（0 = 默认值，1 = 不对齐）
                // 10.输出PCM文件
                fwrite(out_buffer, 1, (size_t) out_buffer_size, fp_pcm);
            }
        }
        // 释放资源
        av_free_packet(packet);
    }

    // 关闭文件
    fclose(fp_pcm);

    // 释放AVFrame
    av_frame_free(&pFrame);

    // 释放缓冲区
    av_free(out_buffer);

    // 释放重采样上下文
    swr_free(&swrCtx);

    // 关闭解码器
    avcodec_close(pCodecCtx);

    // 关闭AVFormatContext
    avformat_close_input(&pFormatCtx);

    (*env)->ReleaseStringUTFChars(env, input_jstr, input_cstr);
    (*env)->ReleaseStringUTFChars(env, output_jstr, output_cstr);

    return 0;
}