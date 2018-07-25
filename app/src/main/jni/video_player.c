#include "com_jamff_ffmpeg_DecodeUtils.h"
#include <stdlib.h>
#include <stdio.h>
#include <android/log.h>

// 编解码
#include <libavcodec/avcodec.h>
// 封装格式处理
#include <libavformat/avformat.h>
// 像素处理
#include <libswscale/swscale.h>

#define LOG_I(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO,"JamFF",FORMAT,##__VA_ARGS__);
#define LOG_E(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"JamFF",FORMAT,##__VA_ARGS__);

JNIEXPORT jint JNICALL
Java_com_jamff_ffmpeg_DecodeUtils_decodeVideo(JNIEnv *env, jclass type, jstring input_jstr,
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

    // 视频解码，需要找到视频对应的AVStream所在pFormatCtx->streams的索引位置
    int video_stream_idx = -1;
    int i;
    // 遍历所有类型的流（音频流、视频流、字幕流）
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        // 根据类型判断，找到视频流
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            // 获取视频流的索引位置
            video_stream_idx = i;
            break;
        }
    }

    if (video_stream_idx == -1) {
        LOG_E("找不到视频流");
        return -1;
    }

    // 视频对应的AVStream
    AVStream *stream = pFormatCtx->streams[video_stream_idx];

    // 通过AVStream可以得到视频帧率，每秒多少帧
    double frame_rate = av_q2d(stream->avg_frame_rate);
    LOG_I("帧率 = %f", frame_rate);

    // 只有知道视频的编码方式，才能够根据编码方式去找到解码器
    // 4.获取视频流中的编解码器上下文
    AVCodecContext *pCodecCtx = stream->codec;// 由libavformat分配和释放

    // 5.根据编解码上下文中的编码id查找对应的视频解码器
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    // 例如加密或者没有该编码的解码器
    if (pCodec == NULL) {
        // 迅雷看看，找不到解码器，临时下载一个解码器
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

    // 获取视频宽高
    int videoWidth = pCodecCtx->width;
    int videoHeight = pCodecCtx->height;

    // 输出视频信息
    LOG_I("多媒体格式：%s", pFormatCtx->iformat->name);
    LOG_I("时长：%f, %f", (pFormatCtx->duration) / 1000000.0,
          stream->duration * av_q2d(stream->time_base));
    LOG_I("视频的宽高：%d, %d", videoWidth, videoHeight);
    LOG_I("解码器的名称：%s", pCodec->name);

    // 准备读取
    // AVPacket，编码数据，用于存储一帧一帧的压缩数据(H264)
    // 缓冲区，开辟空间
    AVPacket *packet = av_malloc(sizeof(AVPacket));
    if (packet == NULL) {
        LOG_E("分配AVPacket内存失败");
        return -1;
    }

    // AVFrame，解码数据，用于输入解码后的像素数据
    // 内存分配
    AVFrame *pFrame = av_frame_alloc();// 必须使用av_frame_free()释放
    if (pFrame == NULL) {
        LOG_E("分配输入AVFrame内存失败");
        return -1;
    }

    // 用于像素格式转码或者缩放
    struct SwsContext *sws_ctx = sws_getContext(videoWidth, videoHeight,// 原始画面宽高
                                                pCodecCtx->pix_fmt,// 原始画面像素格式
                                                videoWidth, videoHeight,// 目标画面宽高
                                                AV_PIX_FMT_YUV420P,// 目标画面像素格式
                                                SWS_BILINEAR,// 算法
                                                NULL, NULL, NULL);

    // AVFrame，YUV420，用于输出解码后的像素数据(YUV)
    AVFrame *pFrameYUV = av_frame_alloc();// 必须使用av_frame_free()释放
    if (pFrameYUV == NULL) {
        LOG_E("分配输出AVFrame内存失败");
        return -1;
    }

    // 只有指定了AVFrame的像素格式、画面大小才能真正分配内存
    // 缓冲区分配内存
    uint8_t *out_buffer = (uint8_t *) av_malloc(
            avpicture_get_size(AV_PIX_FMT_YUV420P, videoWidth, videoHeight));
    // 根据参数，初始化缓冲区
    int size = avpicture_fill((AVPicture *) pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P,
                              videoWidth, videoHeight);

    if (size < 0) {
        LOG_E("初始化缓冲区失败");
        return -1;
    }

    // 输出文件
    FILE *fp_yuv = fopen(output_cstr, "wb+");

    int got_picture, ret;

    int frame_count = 0;

    // 7.一帧一帧的读取压缩视频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        // 只要视频压缩数据（根据流的索引位置判断）
        if (packet->stream_index == video_stream_idx) {
            // 8.解码一帧视频压缩数据，得到视频像素数据，AVPacket->AVFrame
            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
            if (ret < 0) {
                LOG_E("解码错误");
                return -1;
            }

            // 为0说明没有帧可以解压缩，非0正在解码
            if (got_picture > 0) {
                // 9.转换视频
                // AVFrame转为像素格式YUV420，宽高，AVFrame->yuvFrame(YUV420P)
                // 2 6输入、输出数据
                // 3 7输入、输出画面一行的数据的大小 AVFrame 转换是一行一行转换的
                // 4 输入数据第一列要转码的位置 从0开始
                // 5 输入画面的高度
                sws_scale(sws_ctx, (const uint8_t *const *) pFrame->data,
                          pFrame->linesize, 0, videoHeight,// 也可以使用pFrame->height，值相等
                          pFrameYUV->data, pFrameYUV->linesize);

                // 10.输出到YUV文件
                // AVFrame像素帧写入文件->YUV
                // data解码后的图像像素数据（音频采样数据）
                int y_size = videoWidth * videoHeight;
                // Y 亮度 UV 色度（压缩了） 人对亮度更加敏感
                // 一个像素包含了一个Y，而U V 个数是Y的1/4
                fwrite(pFrameYUV->data[0], 1, (size_t) y_size, fp_yuv);
                fwrite(pFrameYUV->data[1], 1, (size_t) (y_size / 4), fp_yuv);
                fwrite(pFrameYUV->data[2], 1, (size_t) (y_size / 4), fp_yuv);

                double cur_time = packet->pts * av_q2d(stream->time_base);
                // double cur_time = pFrame->pts * av_q2d(stream->time_base);// 值相等，拷贝自AVPacket的pts
                LOG_I("解码%d帧, %f秒", ++frame_count, cur_time);
            }
        }
        // 释放资源
        av_free_packet(packet);
    }

    // 关闭文件
    fclose(fp_yuv);

    // 释放SwsContext
    sws_freeContext(sws_ctx);

    // 释放缓冲区
    av_free(out_buffer);

    // 释放AVFrame
    av_frame_free(&pFrame);
    av_frame_free(&pFrameYUV);

    // 关闭解码器
    avcodec_close(pCodecCtx);

    // 关闭AVFormatContext
    avformat_close_input(&pFormatCtx);

    (*env)->ReleaseStringUTFChars(env, input_jstr, input_cstr);
    (*env)->ReleaseStringUTFChars(env, output_jstr, output_cstr);

    return 0;
}