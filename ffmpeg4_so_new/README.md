# FFmpeg最新API

1. 在NDK r17b环境下，使用新脚本，编译FFmpeg4.0.1动态库
2. 使用FFmpeg最新API解码视频

### Linux下FFmpeg编译脚本

##### 编译armeabi-v7a的脚本

```bash
#!/bin/bash
#shell脚本第一行必须是指定shell脚本解释器，这里使用的是bash解释器

#一系列命令的集合 cd xx;dir

#ndk r17b

make clean

#指令的集合

#执行ffmpeg 配置脚本
#--prefix 安装、输出目录 so、a的输出目录 javac -o
#--enable-cross-compile 开启交叉编译器
#--cross-prefix 编译工具前缀 指定ndk中的工具给这个参数
#--disable-shared 关闭动态库
#--disable-programs 关闭程序的编译 ffmpeg ffplay
#--extra-cflags 给编译的参数

export NDK=/usr/ndk/android-ndk-r17b
export SYSROOT=$NDK/platforms/android-21/arch-arm/
export TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64
export CPU=arm
export PREFIX=$(pwd)/android/$CPU

#给编译器的变量
#定义变量 值从FFmpeg_Player\app\.externalNativeBuild\cmake\release\armeabi-v7a\build.ninja 复制的
FLAG="-isystem $NDK/sysroot/usr/include/arm-linux-androideabi -D__ANDROID_API__=21 -g -DANDROID -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb -Wa,--noexecstack -Wformat -Werror=format-security  -Os -DNDEBUG  -fPIC"

INCLUDES="-isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/include -isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a/include -isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/include/backward"

./configure \
--prefix=$PREFIX \
--enable-cross-compile \
--cross-prefix=$TOOLCHAIN/bin/arm-linux-androideabi- \
--target-os=android \
--arch=arm \
--enable-shared \
--disable-static \
--disable-programs \
--sysroot=$SYSROOT \
--extra-cflags="-isysroot $NDK/sysroot $FLAG $INCLUDES" \

make clean
make install
```

##### 编译arm64-v8a的脚本

```bash
#!/bin/bash
#shell脚本第一行必须是指定shell脚本解释器，这里使用的是bash解释器

#一系列命令的集合 cd xx;dir

#ndk r17b

make clean

#指令的集合

#执行ffmpeg 配置脚本
#--prefix 安装、输出目录 so、a的输出目录 javac -o
#--enable-cross-compile 开启交叉编译器
#--cross-prefix 编译工具前缀 指定ndk中的工具给这个参数
#--disable-shared 关闭动态库
#--disable-programs 关闭程序的编译 ffmpeg ffplay
#--extra-cflags 给编译的参数

export NDK=/usr/ndk/android-ndk-r17b
export SYSROOT=$NDK/platforms/android-21/arch-arm64/
export TOOLCHAIN=$NDK/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64
export CPU=arm64
export PREFIX=$(pwd)/android/$CPU

#给编译器的变量
#定义变量 值从FFmpeg_Player\app\.externalNativeBuild\cmake\release\arm64-v8a\build.ninja 复制的
FLAG="-isystem $NDK/sysroot/usr/include/aarch64-linux-android -D__ANDROID_API__=21 -g -DANDROID -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -Wa,--noexecstack -Wformat -Werror=format-security  -O2 -DNDEBUG  -fPIC"

INCLUDES="-isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/include -isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/libs/arm64-v8a/include -isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/include/backward"

./configure \
--prefix=$PREFIX \
--enable-cross-compile \
--cross-prefix=$TOOLCHAIN/bin/aarch64-linux-android- \
--target-os=android \
--arch=aarch64 \
--enable-shared \
--disable-static \
--disable-programs \
--sysroot=$SYSROOT \
--extra-cflags="-isysroot $NDK/sysroot $FLAG $INCLUDES" \

make clean
make install
```

对比ffmpeg4_so的脚本，编译结果少了libpostproc模块，添加--enable-gpl编译即可

### 使用FFmpeg最新API

##### ~~av_register_all()~~

废弃，不需要在入口调用`av_register_all()`进行注册所有组件。

##### ~~pFormatCtx->streams[i]->codec~~->codec_type

废弃，推荐使用`pFormatCtx->streams[i]->codecpar->codec_type`

##### 获取AVCodecContext的方式

* 老方式
    
    ```c
    // 视频对应的AVStream
    AVStream *stream = pFormatCtx->streams[video_stream_idx];
    
    // 视频帧率，每秒多少帧
    double frame_rate = av_q2d(stream->avg_frame_rate);
    LOG_I("帧率 = %f", frame_rate);
    
    // 只有知道视频的编码方式，才能够根据编码方式去找到解码器
    // 4.获取视频流中的编解码器上下文
    AVCodecContext *pCodecCtx = stream->codec;
    
    // 5.根据编解码上下文中的编码id查找对应的视频解码器
    AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    // 例如加密或者没有该编码的解码器
    if (pCodec == NULL) {
        // 迅雷看看，找不到解码器，临时下载一个解码器
        LOG_E("找不到解码器");
        return -1;
    }
    ```

* 新方式

    ```c
    // 视频对应的AVStream
    AVStream *stream = pFormatCtx->streams[video_stream_idx];

    // 视频帧率，每秒多少帧
    double frame_rate = av_q2d(stream->avg_frame_rate);
    LOG_I("帧率 = %f", frame_rate);

    // 4.只有知道视频的编码方式，才能够根据编码方式去找到解码器
    // 事实上AVCodecParameters包含了大部分解码器相关的信息
    AVCodecParameters *pCodecParam = stream->codecpar;
    // 根据AVCodecParameters中的编码id查找对应的视频解码器
    AVCodec *pCodec = avcodec_find_decoder(pCodecParam->codec_id);
    // 例如加密或者没有该编码的解码器
    if (pCodec == NULL) {
        // 迅雷看看，找不到解码器，临时下载一个解码器
        LOG_E("找不到解码器");
        return;
    }

    // 初始化编解码上下文
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);// 需要使用avcodec_free_context释放
    // 这里是直接从AVCodecParameters复制到AVCodecContext
    avcodec_parameters_to_context(pCodecCtx, pCodecParam);
    ```

##### ~~avcodec_decode_video2()~~

废弃，推荐使用`avcodec_send_packet()`和`avcodec_receive_frame()`

```c
ret = avcodec_send_packet(pCodecCtx, packet);
if (ret < 0) {
    LOG_E("Error while sending a packet to the decoder");
    break;
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
            // TODO
            break;
        default:// 合法的解码错误
            LOG_E("Error while receiving a frame from the decoder");
            break;
    }
}
```

##### ~~av_free_packet()~~

废弃，推荐使用`av_packet_unref()`

### 参考

[FFmpeg Documentation](http://ffmpeg.org/doxygen/trunk/index.html)
[av_register_all is deprecated](https://github.com/intel/libyami-utils/pull/118)
[FFmpeg新旧接口对照使用笔记](https://blog.csdn.net/zhangwu1241/article/details/53183590)
[ffmpeg 新老接口问题及对照集锦](https://blog.csdn.net/sukhoi27smk/article/details/18842725)
[用AVCodecParameters代替AVCodecContext](https://blog.csdn.net/luotuo44/article/details/54981809)
[ffmpeg api升级到3.3 api变化](https://www.cnblogs.com/elesos/p/6866599.html)