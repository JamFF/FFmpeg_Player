# FFmpeg解码音视频

1. 在NDK r10e环境下，使用老脚本，编译FFmpeg2.6.9动态库
2. 将MP3解码PCM，可支持其他格式音频和视频
3. 将MP4解码YUV，可以支持其他视频，例如mkv，avi，flv

## Linux下FFmpeg编译脚本

在android-21之前不支持arm64架构，这里只编译arm架构

```bash
#!/bin/bash
make clean
#export后面的变量是全局变量，多个shell脚本都可使用
export NDK=/usr/ndk/android-ndk-r10e
export SYSROOT=$NDK/platforms/android-9/arch-arm/
export TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-4.8/prebuilt/linux-x86_64
export CPU=arm
export PREFIX=$(pwd)/android/$CPU
export ADDI_CFLAGS="-marm"

./configure --target-os=linux \
--prefix=$PREFIX --arch=arm \
--disable-doc \
--enable-shared \
--disable-static \
--disable-yasm \
--disable-symver \
--enable-gpl \
--disable-ffmpeg \
--disable-ffplay \
--disable-ffprobe \
--disable-ffserver \
--disable-doc \
--disable-symver \
--cross-prefix=$TOOLCHAIN/bin/arm-linux-androideabi- \
--enable-cross-compile \
--sysroot=$SYSROOT \
--extra-cflags="-Os -fpic $ADDI_CFLAGS" \
--extra-ldflags="$ADDI_LDFLAGS" \
$ADDITIONAL_CONFIGURE_FLAG
make clean
make
make install
```

`doc` ：文档  
`ffplay` ：多媒体播放器  
`ffprobe` ：查看多媒体信息  
`ffserver` ：流媒体服务器  
`cross-compile` ：交叉编译  
`cross-prefix` ：使用交叉编译需要为其配置路径  
`sysroot` ：指定了逻辑目录

## 音频解码

### Native方法

```java
/**
 * 音频解码
 *
 * @param input  输入音频路径
 * @param output 解码后PCM保存路径
 * @return 0成功，-1失败
 */
public native static int decodeAudio(String input, String output);
```

### 解码流程

1. 注册所有组件

    初始化`libavformat`并注册所有组件
    ```c
    av_register_all();
    ```
    
2. 打开输入媒体文件

    1. 打开输入流并读取header
    
        ```c
        // 第一个参数：AVFormatContext的二级指针，由avformat_alloc_context分配
        // 第二个参数：输入流的URL
        // 第三个参数：为NULL表示自动检测输入格式
        int av_error = avformat_open_input(&pFormatCtx, input_cstr, NULL, NULL);
        if (av_error != 0) {
            LOG_E("error:%d, %s,", av_error, "无法打开输入音频文件");
            return -1;
        }
        ```
        
        注意：该流必须用`avformat_close_input()`关闭。
    
    2. 分配一个AVFormatContext
    
        ```c
        // 封装格式上下文，统领全局的结构体，保存了媒体文件封装格式的相关信息
        AVFormatContext *pFormatCtx = avformat_alloc_context();
        ```
        
       可以使用`avformat_free_context()`释放。
    
    3. 在程序执行最后关闭AVFormatContext
    
        ```c
        // 关闭AVFormatContext，释放它及其所有内容并将pFormatCtx设为NULL
        avformat_close_input(&pFormatCtx);
        ```
        
        `avformat_close_input()`主要做了以下几步工作：
        1. 调用`AVInputFormat`的`read_close()`方法关闭输入流
        2. 调用`avformat_free_context()`释放`AVFormatContext`
        3. 调用`avio_close()`关闭并且释放`AVIOContext`
        
        所以不需要重复调用`avformat_free_context()`释放了。

3. 获取媒体文件流信息

    读取媒体文件的数据包以获取流信息
    
    ```c
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOG_E("无法获取媒体文件信息");
        return -1;
    }
    ```

4. 获取编解码器上下文

    1. 找到音频流的索引位置
    
        ```c
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
        ```

    2. 得到音频对应的AVStream

        ```c
        AVStream *stream = pFormatCtx->streams[audio_stream_idx];
        ```
        
        会在`avformat_free_context()`中由`libavformat`释放。

    3. 再获取音频流中的编解码器上下文
    
        ```c
        AVCodecContext *pCodecCtx = stream->codec;
        ```
        
        由`libavformat`分配和释放。
        
5. 获取解码器

    ```c
    // 根据编解码器上下文中的编码id查找对应的音频解码器
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    // 例如加密或者没有该编码的解码器
    if (pCodec == NULL) {
        LOG_E("找不到解码器");
        return -1;
    }
    ```
    
6. 打开解码器

    使用AVCodec初始化AVCodecContext
    ```c
    // 第一个参数：要初始化的编解码器上下文
    // 第二个参数：从编解码器上下文中得到的编解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOG_E("解码器无法打开");
        return -1;
    }
    ```

7. 读取压缩数据

    ```c
    // 准备读取
    // AVPacket，编码数据，用于存储一帧一帧的压缩数据
    // 缓冲区，开辟空间
    AVPacket *packet = av_malloc(sizeof(AVPacket));
    if (packet == NULL) {
        LOG_E("分配AVPacket内存失败");
        return -1;
    }
    
    // 需要一帧一帧的读取压缩音频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0) {

    }
    ```

8. 解码音频压缩数据

    1. 创建AVFrame，解码数据，用于存储解码后的PCM数据
    
        ```c
        // 内存分配
        AVFrame *pFrame = av_frame_alloc();
        if (pFrame == NULL) {
            LOG_E("分配输入AVFrame内存失败");
            return -1;
        }
        ```
        
        必须使用`av_frame_free()`释放。
    
    2. 解码
    
        ```c
        int got_frame, ret;
    
        // 需要一帧一帧的读取压缩音频数据AVPacket
        while (av_read_frame(pFormatCtx, packet) >= 0) {
            // 只要音频压缩数据（根据流的索引位置判断）
            if (packet->stream_index == audio_stream_idx) {
                // 解码一帧音频压缩数据，得到音频PCM数据，AVPacket->AVFrame
                ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_frame, packet);
                if (ret < 0) {
                    LOG_E("解码错误 %d", ret);
                    // FIXME 第一帧会返回-1094995529，AVERROR_INVALIDDATA
                    // return -1;
                }
        
                // 为0说明没有帧可以解压缩，非0正在解码
                if (got_frame) {
        
                }
                // 释放资源
                av_free_packet(packet);
            }
        }
        ```

9. 转换音频

    参考ffmpeg示例程序`ffmpeg-2.6.9\doc\examples\resampling_audio.c`，使用`swr_convert()`
    
    1. 分配重采样上下文，并设置参数
    
        1. 创建重采样上下文
    
            ```c
            SwrContext *swrCtx = swr_alloc();
            if (swrCtx == NULL) {
                LOG_E("分配SwrContext失败");
            }
            ```
            
            使用`swr_alloc()`需要在调用`swr_init()`之前，手动或使用`swr_alloc_set_opts()`设置参数。

        2. 重采样设置参数
        
            ```c
            // 输出的声道布局（立体声）
            uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;
        
            // 输出采样格式，16bit
            enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
        
            // 输出采样率，44100Hz，在一秒钟内对声音信号采样44100次
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
            ```
    
        3. 根据设置的参数，初始化重采样上下文
        
            ```c
            swr_init(swrCtx);
            ```

    2. 输出缓冲区
        ```c
        #define MAX_AUDIO_FRAME_SIZE 44100 * 2
        // 16bit 44100 PCM 数据，16bit是2个字节
        uint8_t *out_buffer = (uint8_t *) av_malloc(MAX_AUDIO_FRAME_SIZE);
        ```

    3. 其他参数
        ```c
        // 为0说明没有帧可以解压缩，非0正在解码
        if (got_frame > 0) {
            // 9.转换音频
            ret = swr_convert(swrCtx,// 重采样上下文
                              &out_buffer,// 输出缓冲区
                              MAX_AUDIO_FRAME_SIZE,// 每通道采样的可用空间量
                              (const uint8_t **) pFrame->data,// 输入缓冲区
                              pFrame->nb_samples);// 一个通道中可用的输入采样数量
            if (ret < 0) {
                LOG_E("转换时出错");
            } else {
                fwrite(out_buffer,1,out_buffer_size,fp_pcm);
            }
        }
        ```
        
10. 输出PCM文件

    1. 输出文件
    
        ```c
        // 输出文件
        FILE *fp_pcm = fopen(output_cstr, "wb");
        ```
        
    2. 输出的声道个数
    
        ```c
        // 输出的声道个数
        int out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);
        ```
    
    3. 获取给定音频参数所需的缓冲区大小
    
        ```c
        int out_buffer_size = av_samples_get_buffer_size(NULL, out_channel_nb,// 输出的声道个数
                                                         pFrame->nb_samples,// 一个通道中音频采样数量
                                                         out_sample_fmt,// 输出采样格式16bit
                                                         1);// 缓冲区大小对齐（0 = 默认值，1 = 不对齐）
        ```
        
    4. 输出PCM文件
    
        ```c
        fwrite(out_buffer, 1, (size_t) out_buffer_size, fp_pcm);
        ```
    
11. 释放资源

    ```c
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
    ```

## 视频解码

与音频解码类似

### Native方法

```java
/**
 * 视频解码
 *
 * @param input  输入视频路径
 * @param output 解码后YUV保存路径
 * @return 0成功，-1失败
 */
public native static int decodeVideo(String input, String output);
```

### 解码流程
 
1. 注册所有组件

    初始化`libavformat`并注册所有组件
    ```c
    av_register_all();
    ```
    
2. 打开输入媒体文件

    1. 打开输入流并读取header
    
        ```c
        // 第一个参数：AVFormatContext的二级指针，由avformat_alloc_context分配
        // 第二个参数：输入流的URL
        // 第三个参数：为NULL表示自动检测输入格式
        int av_error = avformat_open_input(&pFormatCtx, input_cstr, NULL, NULL);
        if (av_error != 0) {
            LOG_E("error:%d, %s,", av_error, "无法打开输入媒体文件");
            return -1;
        }
        ```
        
        注意：该流必须用`avformat_close_input()`关闭。
    
    2. 分配一个AVFormatContext
    
        ```c
        // 封装格式上下文，统领全局的结构体，保存了媒体文件封装格式的相关信息
        AVFormatContext *pFormatCtx = avformat_alloc_context();
        ```
        
       可以使用`avformat_free_context()`释放。
    
    3. 在程序执行最后关闭AVFormatContext
    
        ```c
        // 关闭AVFormatContext，释放它及其所有内容并将pFormatCtx设为NULL
        avformat_close_input(&pFormatCtx);
        ```
        
        `avformat_close_input()`主要做了以下几步工作：
        1. 调用`AVInputFormat`的`read_close()`方法关闭输入流
        2. 调用`avformat_free_context()`释放`AVFormatContext`
        3. 调用`avio_close()`关闭并且释放`AVIOContext`
        
        所以不需要重复调用`avformat_free_context()`释放了。

3. 获取媒体文件流信息

    读取媒体文件的数据包以获取流信息
    
    ```c
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        LOG_E("无法获取媒体文件信息");
        return -1;
    }
    ```

4. 获取编解码器上下文

    1. 找到视频流的索引位置
    
        ```c
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
        ```

    2. 得到视频对应的AVStream

        ```c
        AVStream *stream = pFormatCtx->streams[video_stream_idx];
        
        // 通过AVStream可以得到视频帧率，每秒多少帧
        double frame_rate = av_q2d(stream->avg_frame_rate);
        LOG_I("帧率 = %f", frame_rate);
        ```
        
        会在`avformat_free_context()`中由`libavformat`释放。

    3. 再获取视频流中的编解码器上下文
    
        ```c
        AVCodecContext *pCodecCtx = stream->codec;
        ```
        
        由`libavformat`分配和释放。
        
5. 获取解码器

    ```c
    // 根据编解码器上下文中的编码id查找对应的音频解码器
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    // 例如加密或者没有该编码的解码器
    if (pCodec == NULL) {
        // 迅雷看看，找不到解码器，临时下载一个解码器
        LOG_E("找不到解码器");
        return -1;
    }
    ```
    
6. 打开解码器

    使用AVCodec初始化AVCodecContext
    ```c
    // 第一个参数：要初始化的编解码器上下文
    // 第二个参数：从编解码器上下文中得到的编解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOG_E("解码器无法打开");
        return -1;
    }
    
    // 可以根据AVCodecContext得到媒体信息
    // 获取视频宽高
    int videoWidth = pCodecCtx->width;
    int videoHeight = pCodecCtx->height;

    // 输出视频信息
    LOG_I("视频的文件格式：%s", pFormatCtx->iformat->name);
    LOG_I("视频时长：%f, %f", (pFormatCtx->duration) / 1000000.0,
          stream->duration * av_q2d(stream->time_base));
    LOG_I("视频的宽高：%d, %d", videoWidth, videoHeight);
    LOG_I("解码器的名称：%s", pCodec->name);
    ```

7. 读取压缩数据

    ```c
    // 准备读取
    // AVPacket，编码数据，用于存储一帧一帧的压缩数据(H264)
    // 缓冲区，开辟空间
    AVPacket *packet = av_malloc(sizeof(AVPacket));
    if (packet == NULL) {
        LOG_E("分配AVPacket内存失败");
        return -1;
    }
    
    // 需要一帧一帧的读取压缩音频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        // 只要视频压缩数据（根据流的索引位置判断）
        if (packet->stream_index == video_stream_idx) {
        
        }
        // 释放资源
        av_free_packet(packet);
    }
    ```

8. 解码视频压缩数据

    1. 创建AVFrame，解码数据，用于输入解码后的像素数据
    
        ```c
        // 内存分配
        AVFrame *pFrame = av_frame_alloc();// 必须使用av_frame_free()释放
        if (pFrame == NULL) {
            LOG_E("分配输入AVFrame内存失败");
            return -1;
        }
        ```
        
        必须使用`av_frame_free()`释放。
    
    2. 解码
    
        ```c
        int got_picture, ret;
    
        // 只要视频压缩数据（根据流的索引位置判断）
        if (packet->stream_index == video_stream_idx) {
            // 解码一帧视频压缩数据，得到视频像素数据，AVPacket->AVFrame
            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
            if (ret < 0) {
                LOG_E("解码错误");
                return -1;
            }

            // 为0说明没有帧可以解压缩，非0正在解码
            if (got_picture > 0) {
               
            }
        }
        ```

9. 转换视频

    参考ffmpeg示例程序`ffmpeg-2.6.9\doc\examples\scaling_video.c`，使用`sws_scale()`
    
    1. 分配SwsContext，并设置参数

        ```c
        // 用于像素格式转码或者缩放
        struct SwsContext *sws_ctx = sws_getContext(videoWidth, videoHeight,// 原始画面宽高
                                                    pCodecCtx->pix_fmt,// 原始画面像素格式
                                                    videoWidth, videoHeight,// 目标画面宽高
                                                    AV_PIX_FMT_YUV420P,// 目标画面像素格式
                                                    SWS_BILINEAR,// 算法
                                                    NULL, NULL, NULL);
        ```

    2. 创建输出的AVFrame
    
        ```c
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
        ```

    3. 将输入的AVFrame转换为YUV格式
    
        ```c
        // AVFrame转为像素格式YUV420，宽高，AVFrame->yuvFrame(YUV420P)
        // 2 6输入、输出数据
        // 3 7输入、输出画面一行的数据的大小 AVFrame 转换是一行一行转换的
        // 4 输入数据第一列要转码的位置 从0开始
        // 5 输入画面的高度
        sws_scale(sws_ctx, (const uint8_t *const *) pFrame->data,
                  pFrame->linesize, 0, videoHeight,// 也可以使用pFrame->height，值相等
                  pFrameYUV->data, pFrameYUV->linesize);
        ```
        
10. 输出YUV文件

    1. 输出文件
    
        ```c
        // 输出文件
        FILE *fp_yuv = fopen(output_cstr, "wb+");
        ```
        
    2. 输出YUV文件
    
        ```c
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
        ```

11. 释放资源

    ```c
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
    ```
    
## 注意

### 解码音频中，第一帧会返回错误码

```c
// 解码一帧音频压缩数据，得到音频PCM数据，AVPacket->AVFrame
ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_frame, packet);
if (ret < 0) {
    LOG_E("解码错误 %d", ret);
    // FIXME 第一帧会返回-1094995529，AVERROR_INVALIDDATA
    // return -1;
}
```

### 解决方式

更新FFmpeg4.0后，`avcodec_decode_audio4`过时

使用`avcodec_send_packet()`和`avcodec_receive_frame()`代替

```c
// 解码一帧音频压缩数据，得到音频PCM数据，AVPacket->AVFrame
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
                fwrite(out_buffer, 1, (size_t) out_buffer_size, fp_pcm);
            }
            break;
        default:// 合法的解码错误
            LOG_E("从解码器接收帧时出错 %d", ret);
            break;
    }
}
```

## 参考

[FFmpeg源代码简单分析：内存的分配和释放](https://blog.csdn.net/leixiaohua1020/article/details/41176777)  
[ffmpeg返回错误码](https://blog.csdn.net/disadministrator/article/details/43987403)  
[FFmpeg avcodec_send_packet函数错误定位](http://blog.51cto.com/fengyuzaitu/2046171)  
[最简单的基于FFmpeg的libswscale的示例（YUV转RGB）](https://blog.csdn.net/leixiaohua1020/article/details/42134965)  
[ffmpeg中的sws_scale算法性能测试](https://blog.csdn.net/leixiaohua1020/article/details/12029505)