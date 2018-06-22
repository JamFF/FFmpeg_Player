#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
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

// YUV转换RGB
#include <libyuv.h>

#define LOG_I(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO,"JamFF",FORMAT,##__VA_ARGS__);
#define LOG_E(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"JamFF",FORMAT,##__VA_ARGS__);

// 暂停的标记位
int flag = 0;

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_render(JNIEnv *env, jobject instance, jstring input_jstr,
                                      jobject surface) {
    // 需要转码的视频文件(输入的视频文件)
    const char *input_cstr = (*env)->GetStringUTFChars(env, input_jstr, 0);

    // 1.注册所有组件
    av_register_all();

    // 封装格式上下文，统领全局的结构体，保存了视频文件封装格式的相关信息
    AVFormatContext *pFormatCtx = avformat_alloc_context();

    // 2.打开输入视频文件
    int av_error = avformat_open_input(&pFormatCtx, input_cstr, NULL, NULL);
    if (av_error != 0) {
        LOG_E("error:%d, %s,", av_error, "无法打开输入视频文件");
        return;
    }

    // 3.获取视频文件信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOG_E("%s", "无法获取视频文件信息");
        return;
    }

    // 视频解码，需要找到视频对应的AVStream所在pFormatCtx->streams的索引位置
    int video_stream_idx = -1;
    int i = 0;
    // 遍历所有类型的流（音频流、视频流、字幕流）
    for (; i < pFormatCtx->nb_streams; i++) {
        // 根据类型判断，找到视频流
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            // 获取视频流的索引位置
            video_stream_idx = i;
            break;
        }
    }

    if (video_stream_idx == -1) {
        LOG_E("%s", "找不到视频流");
        return;
    }

    // 只有知道视频的编码方式，才能够根据编码方式去找到解码器
    // 获取视频流中的编解码上下文
    AVCodecContext *pCodecCtx = pFormatCtx->streams[video_stream_idx]->codec;
    // 4.根据编解码上下文中的编码id查找对应的视频解码器
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    // 例如加密或者没有该编码的解码器
    if (pCodec == NULL) {
        // 迅雷看看，找不到解码器，临时下载一个解码器
        LOG_E("%s", "找不到解码器");
        return;
    }

    // 5.打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOG_E("%s", "解码器无法打开");
        return;
    }

    // 输出视频信息
    LOG_I("视频的文件格式：%s", pFormatCtx->iformat->name);
    LOG_I("视频时长：%lld", (pFormatCtx->duration) / 1000000);
    LOG_I("视频的宽高：%d,%d", pCodecCtx->width, pCodecCtx->height);
    LOG_I("解码器的名称：%s", pCodec->name);

    // 准备读取
    // AVPacket，编码数据，用于存储一帧一帧的压缩数据（H264）
    // 缓冲区，开辟空间
    AVPacket *packet = av_malloc(sizeof(AVPacket));

    // AVFrame，像素数据（解码数据），用于存储解码后的像素数据(YUV)
    // 内存分配
    AVFrame *pFrame = av_frame_alloc();// 实际就是YUV420P

    /************************************* native绘制 start *************************************/
    AVFrame *pRGBFrame = av_frame_alloc();// RGB

    // 1、获取一个关联Surface的NativeWindow窗体
    ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
    // 绘制时的缓冲区
    ANativeWindow_Buffer outBuffer;

    flag = 0;
    /************************************* native绘制 end ***************************************/

    int got_picture, ret;

    int frame_count = 0;

    // 6.一帧一帧的读取压缩视频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0 && flag == 0) {
        // 只要视频压缩数据（根据流的索引位置判断）
        if (packet->stream_index == video_stream_idx) {
            // 7.解码一帧视频压缩数据，得到视频像素数据，AVPacket->AVFrame
            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
            if (ret < 0) {
                LOG_E("%s", "解码错误");
                return;
            }

            // 为0说明解码完成，非0正在解码
            if (got_picture) {
                LOG_I("解码%d帧", ++frame_count);

                // 2、设置缓冲区的属性（宽、高、像素格式），像素格式要和SurfaceView的像素格式一直
                ANativeWindow_setBuffersGeometry(nativeWindow, pCodecCtx->width, pCodecCtx->height,
                                                 WINDOW_FORMAT_RGBA_8888);

                // 3、lock锁定下一个即将要绘制的Surface
                ANativeWindow_lock(nativeWindow, &outBuffer, NULL);

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
                ANativeWindow_unlockAndPost(nativeWindow);

                usleep(1000 * 30);
            }
        }

        // 释放资源
        av_free_packet(packet);
    }

    // 7、释放资源
    ANativeWindow_release(nativeWindow);

    // 释放AVFrame
    av_frame_free(&pFrame);
    av_frame_free(&pRGBFrame);

    // 关闭解码器
    avcodec_close(pCodecCtx);

    // 释放AVFormatContext
    avformat_free_context(pFormatCtx);

    (*env)->ReleaseStringUTFChars(env, input_jstr, input_cstr);
}

JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_stop(JNIEnv *env, jobject instance) {
    flag = -1;
}