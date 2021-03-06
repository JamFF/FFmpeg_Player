# FFmpeg播放音视频

1. 在NDK r17环境下，使用老脚本，编译FFmpeg4.0动态库
2. 在C代码中开启子线程，将MP4解码YUV，转为RGB并绘制到UI，可以支持其他视频，例如mkv，avi，flv
3. 在C代码中开启子线程，播放MP3文件，可支持其他格式音频和视频

## Linux下FFmpeg编译脚本

### 编译armeabi-v7a的脚本

```bash
#!/bin/bash
#shell脚本第一行必须是指定shell脚本解释器，这里使用的是bash解释器

#一系列命令的集合 cd xx;dir

#ndk r17

make clean

#指令的集合

#执行ffmpeg 配置脚本
#--prefix 安装、输出目录 so、a的输出目录 javac -o
#--enable-cross-compile 开启交叉编译器
#--cross-prefix 编译工具前缀 指定ndk中的工具给这个参数
#--disable-shared 关闭动态库
#--disable-programs 关闭程序的编译 ffmpeg ffplay
#--extra-cflags 给编译的参数

export NDK=/usr/ndk/android-ndk-r17
export SYSROOT=$NDK/platforms/android-21/arch-arm/
export TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64
export CPU=arm
export PREFIX=$(pwd)/android/$CPU

#给编译器的变量
#定义变量 值从FFmpeg_Player\app\.externalNativeBuild\cmake\release\armeabi-v7a\build.ninja 复制的
FLAG="-isystem $NDK/sysroot/usr/include/arm-linux-androideabi -D__ANDROID_API__=21 -g -DANDROID -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb -Wa,--noexecstack -Wformat -Werror=format-security  -Os -DNDEBUG  -fPIC"

INCLUDES="-isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/include -isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a/include -isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/include/backward"

./configure \
--target-os=android \
--prefix=$PREFIX \
--arch=arm \
--enable-shared \
--disable-static \
--disable-yasm \
--enable-gpl \
--disable-ffmpeg \
--disable-ffplay \
--disable-ffprobe \
--disable-doc \
--disable-symver \
--cross-prefix=$TOOLCHAIN/bin/arm-linux-androideabi- \
--enable-cross-compile \
--sysroot=$SYSROOT \
--extra-cflags="-isysroot $NDK/sysroot $FLAG $INCLUDES" \
--extra-ldflags="$ADDI_LDFLAGS" \
$ADDITIONAL_CONFIGURE_FLAG
make clean
make
make install
```

### 编译arm64-v8a的脚本

```bash
#!/bin/bash
#shell脚本第一行必须是指定shell脚本解释器，这里使用的是bash解释器

#一系列命令的集合 cd xx;dir

#ndk r17

make clean

#指令的集合

#执行ffmpeg 配置脚本
#--prefix 安装、输出目录 so、a的输出目录 javac -o
#--enable-cross-compile 开启交叉编译器
#--cross-prefix 编译工具前缀 指定ndk中的工具给这个参数
#--disable-shared 关闭动态库
#--disable-programs 关闭程序的编译 ffmpeg ffplay
#--extra-cflags 给编译的参数

export NDK=/usr/ndk/android-ndk-r17
export SYSROOT=$NDK/platforms/android-21/arch-arm64/
export TOOLCHAIN=$NDK/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64
export CPU=arm64
export PREFIX=$(pwd)/android/$CPU

#给编译器的变量
#定义变量 值从FFmpeg_Player\app\.externalNativeBuild\cmake\release\arm64-v8a\build.ninja 复制的                                                                                      
FLAG="-isystem $NDK/sysroot/usr/include/aarch64-linux-android -D__ANDROID_API__=21 -g -DANDROID -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -Wa,--noexecstack -Wformat -Werror=format-security  -O2 -DNDEBUG  -fPIC"

INCLUDES="-isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/include -isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/libs/arm64-v8a/include -isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/include/backward"

./configure \
--target-os=android \
--prefix=$PREFIX \
--arch=aarch64 \
--enable-shared \
--disable-static \
--disable-yasm \
--enable-gpl \
--disable-ffmpeg \
--disable-ffplay \
--disable-ffprobe \
--disable-doc \
--disable-symver \
--cross-prefix=$TOOLCHAIN/bin/aarch64-linux-android- \
--enable-cross-compile \
--sysroot=$SYSROOT \
--extra-cflags="-isysroot $NDK/sysroot $FLAG $INCLUDES" \
--extra-ldflags="$ADDI_LDFLAGS" \
$ADDITIONAL_CONFIGURE_FLAG
make clean
make
make install 
```

## 视频播放

之前播放是在Android代码中放入IO线程，避免UI线程阻塞，标注的做法是，在C中的子线程中完成视频的切换，下面就是在将 `22.像素格式转换与Native原生绘制` 的代码放在C子线程中。

### native方法

```java
public class MyPlayer {

    interface OnCompletionListener {
        /**
         * 播放完成的回调
         *
         * @param result 目前为0，后期可加入错误码
         */
        void onCompletion(int result);
    }

    public MyPlayer() {
        init();
    }

    private OnCompletionListener mOnCompletionListener;

    public void setOnCompletionListener(OnCompletionListener onCompletionListener) {
        mOnCompletionListener = onCompletionListener;
    }

    private AudioTrack mAudioTrack;

    public AudioTrack getAudioTrack() {
        return mAudioTrack;
    }

    /**
     * 初始化播放器
     */
    public native void init();

    /**
     * 使用libyuv将YUV转换为RGB，进行播放
     * 部分格式播放时，会出现花屏
     *
     * @param input 输入视频路径
     * @param surface {@link android.view.Surface}
     */
    public native void renderVideo(String input, Object surface);

    /**
     * 使用ffmpeg自带的swscale.h中的sws_scale将解码数据转换为RGB，进行播放
     * 不会出现花屏
     *
     * @param input 输入视频路径
     * @param surface {@link android.view.Surface}
     */
    public native void playVideo(String input, Object surface);

    /**
     * 播放音视频
     *
     * @param input 媒体文件路径
     * @param surface {@link android.view.Surface}
     */
    public native void playMusic(String input, Object surface);

    /**
     * 停止播放
     */
    public native void stop();

    /**
     * 释放播放器资源
     */
    public native void destroy();

    /**
     * 创建AudioTrack，提供给C调用
     *
     * @param sampleRateInHz 采样率
     * @param nb_channels 声道数
     * @return {@link android.media.AudioTrack}
     */
    public AudioTrack createAudioTrack(int sampleRateInHz, int nb_channels) {

        // 固定的音频数据格式，16位PCM
        int audioFormat = AudioFormat.ENCODING_PCM_16BIT;

        // 声道布局
        int channelConfig;
        if (nb_channels == 1) {
            channelConfig = android.media.AudioFormat.CHANNEL_OUT_MONO;
        } else {
            channelConfig = android.media.AudioFormat.CHANNEL_OUT_STEREO;
        }

        // AudioTrack所需的最小缓冲区大小
        int bufferSizeInBytes = AudioTrack.getMinBufferSize(
                sampleRateInHz,// 采样率
                channelConfig,// 声道
                audioFormat);// 音频数据的格式

        mAudioTrack = new AudioTrack(
                AudioManager.STREAM_MUSIC,// 音频流类型
                sampleRateInHz,// 采样率
                channelConfig,// 声道配置
                audioFormat,// 音频数据的格式
                bufferSizeInBytes,// 缓冲区大小
                AudioTrack.MODE_STREAM);// 媒体流MODE_STREAM或静态缓冲MODE_STATIC

        return mAudioTrack;
    }

    /**
     * 播放完成，或者手动停止，提供给C调用
     *
     * @param result 目前为0，后期可加入错误码
     */
    public void onCompletion(int result) {
        if (mOnCompletionListener != null) {
            mOnCompletionListener.onCompletion(result);
        }
    }

    static {
        System.loadLibrary("player");
    }
}
```

### Android方法

```java
/**
 * 播放媒体文件的视频
 *
 * @param is_libyuv true 使用libyuv将YUV转换为RGB
 * false 使用ffmpeg自带的swscale.h中的sws_scale将解码数据转换为RGB
 */
private void playVideo(boolean is_libyuv) {

    if (!checkVideoFile()) {
        return;
    }

    bt_play_video_1.setEnabled(false);
    bt_play_video_2.setEnabled(false);
    bt_play_music.setEnabled(false);
    bt_stop.setEnabled(true);

    if (mSurface == null) {
        Log.e(TAG, "start: mSurface == null");
        return;
    }

    // 子线程播放，播放完成后回调onCompletion
    if (is_libyuv) {
        mPlayer.renderVideo(videoFile.getAbsolutePath(), mSurface);
    } else {
        mPlayer.playVideo(videoFile.getAbsolutePath(), mSurface);
    }
}
```

### NDK方法
##### 方式一

1. jni函数
```c
JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_renderVideo(JNIEnv *env, jobject instance, jstring input_jstr,
                                           jobject surface) {
    LOG_I("renderVideo");

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

    // 创建子线程，解码视频
    pthread_create(&(player->decode_threads[video_stream_index]), NULL, decode_data,
                   (void *) player);

    // 创建子线程，解码音频
    /*pthread_create(&(player->decode_threads[audio_stream_index]), NULL, decode_data,
                   (void *) player);*/

    (*env)->ReleaseStringUTFChars(env, input_jstr, input_cstr);
}
```

2. 封装结构体，保存媒体文件的属性
```c
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
```

3. 初始化封装格式上下文，获取音频视频流的索引位置
```c
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
```

4. 初始化解码器上下文
```c
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
```

5. 解码视频准备工作
```c
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
```

6. 解码音视频，子线程函数
```c
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

    flag = 1;

    // 准备读取
    // AVPacket，编码数据，用于存储一帧一帧的压缩数据（H264）
    // 缓冲区，开辟空间
    AVPacket *packet = av_malloc(sizeof(AVPacket));
    if (packet == NULL) {
        LOG_E("分配AVPacket内存失败");
        goto end;
    }

    // 7.一帧一帧的读取压缩视频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0 && flag) {
        if (packet->stream_index == player->video_stream_index) {
            // 视频压缩数据（根据流的索引位置判断）
            LOG_I("解码视频%d帧", ++frame_count_video);
            decode_video(player, packet, pCodecCtx_video, pFrame, pRGBFrame);
        } else if (packet->stream_index == player->audio_stream_index) {
            // 音频压缩数据（根据流的索引位置判断）
            // LOG_I("解码视频%d帧", ++frame_count_audio);
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

    // 关闭解码器
    avcodec_close(pCodecCtx_video);

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
```

7. 解码视频
```c
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
```

##### 方式二

1. jni函数
```c
JNIEXPORT void JNICALL
Java_com_jamff_ffmpeg_MyPlayer_playVideo(JNIEnv *env, jobject instance, jstring input_jstr,
                                         jobject surface) {
    LOG_I("playVideo");

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

    // 创建子线程，解码视频
    pthread_create(&(player->decode_threads[video_stream_index]), NULL, decode_data2,
                   (void *) player);

    (*env)->ReleaseStringUTFChars(env, input_jstr, input_cstr);
}
```
2. 封装结构体，保存媒体文件的属性，同方式一
3. 初始化封装格式上下文，获取音频视频流的索引位置，同方式一
4. 初始化解码器上下文，同方式一
5. 解码视频准备工作，同方式一
6. 解码音视频，子线程函数
```c
/**
 * 解码音视频，子线程函数
 */
void *decode_data2(void *arg) {

    // 每个线程都有独立的JNIEnv
    JNIEnv *env;
    // 通过JavaVM关联当前线程，获取当前线程的JNIEnv
    (*javaVM)->AttachCurrentThread(javaVM, &env, NULL);

    struct Player *player = (struct Player *) arg;
    AVFormatContext *pFormatCtx = player->input_format_ctx;
    AVCodecContext *pCodecCtx_video = player->input_codec_ctx[player->video_stream_index];

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

    flag = 1;

    // 准备读取
    // AVPacket，编码数据，用于存储一帧一帧的压缩数据（H264）
    // 缓冲区，开辟空间
    AVPacket *packet = av_malloc(sizeof(AVPacket));
    if (packet == NULL) {
        LOG_E("分配AVPacket内存失败");
        goto end;
    }

    // 7.一帧一帧的读取压缩视频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0 && flag) {
        if (packet->stream_index == player->video_stream_index) {
            // 只要视频压缩数据（根据流的索引位置判断）
            /*LOG_I("解码%d帧", ++frame_count_video);
            decode_video2(player, packet, pCodecCtx_video, pFrame, pRGBFrame, sws_ctx);*/
        } else if (packet->stream_index == player->audio_stream_index) {
            // 音频压缩数据（根据流的索引位置判断）
            LOG_I("解码视频%d帧", ++frame_count_audio);
        }
        // 释放资源
        av_packet_unref(packet);// av_packet_unref代替过时的av_free_packet
    }

    end:
    // 释放SwsContext
    sws_freeContext(sws_ctx);

    // 释放缓冲区
    av_free(buffer);
    av_free(out_buffer);

    // 7、释放资源
    ANativeWindow_release(player->nativeWindow);

    // 释放AVFrame
    av_frame_free(&pFrame);
    av_frame_free(&pRGBFrame);

    // 关闭解码器
    avcodec_close(pCodecCtx_video);

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
```

7. 解码视频
```c
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
```

8. 其他jni方法
```c
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
Java_com_jamff_ffmpeg_MyPlayer_destroy(JNIEnv *env, jobject instance) {
    // 释放全局引用，主线程创建的全局引用，必须在主线程释放，子线程释放报错
    (*env)->DeleteGlobalRef(env, player_global);
}
```

## 音频播放

之前播放是在Android代码中放入IO线程，避免UI线程阻塞，标注的做法是，在C中的子线程中完成视频的切换，下面就是在将 24.AudioTrack音频播放 的代码放在C子线程中。

