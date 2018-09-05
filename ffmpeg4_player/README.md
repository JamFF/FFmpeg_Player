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

### UI

1. 绘制到UI上，需要使用`SurfaceView`

    ```java
    /**
     * 描述：视频绘制的SurfaceView
     * 作者：JamFF
     * 创建时间：2018/6/21 14:45
     */
    public class VideoView extends SurfaceView {

        public VideoView(Context context) {
            this(context, null);
        }

        public VideoView(Context context, AttributeSet attrs) {
            this(context, attrs, 0);
        }

        public VideoView(Context context, AttributeSet attrs, int defStyleAttr) {
            super(context, attrs, defStyleAttr);
            init();
        }

        private void init() {
            SurfaceHolder surfaceHolder = getHolder();
            // 初始化SurfaceView绘制的像素格式
            surfaceHolder.setFormat(PixelFormat.RGBA_8888);
        }
    }
    ```

2. 布局

    ```xml
    <?xml version="1.0" encoding="utf-8"?>
    <FrameLayout
        xmlns:android="http://schemas.android.com/apk/res/android"
        xmlns:tools="http://schemas.android.com/tools"
        android:layout_width="match_parent"
        android:layout_height="match_parent"
        tools:context=".MainActivity">

        <com.jamff.ffmpeg.widget.VideoView
            android:id="@+id/video_view"
            android:layout_width="match_parent"
            android:layout_height="match_parent"/>

        <LinearLayout
            android:layout_width="match_parent"
            android:layout_height="wrap_content"
            android:orientation="horizontal">

            <Button
                android:id="@+id/bt_play_video_1"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:enabled="false"
                android:text="@string/play_video_1"/>

            <Button
                android:id="@+id/bt_play_video_2"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:enabled="false"
                android:text="@string/play_video_2"/>

            <Button
                android:id="@+id/bt_stop"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:enabled="false"
                android:text="@string/stop"/>

        </LinearLayout>

    </FrameLayout>
    ```

### 绘制

1. 将文件路径以及Surface传入

    ```java
    /**
     * 视频播放
     *
     * @param id 区分两种YUV420P->RGBA_8888的方式
     */
    private void playVideo(final int id) {

        bt_play_video_1.setEnabled(false);
        bt_play_video_2.setEnabled(false);
        bt_play_music.setEnabled(false);
        bt_stop.setEnabled(true);

        if (mSurface == null) {
            Log.e(TAG, "start: mSurface == null");
            return;
        }

        new Thread(new Runnable() {
            @Override
            public void run() {
                int result;
                if (id == R.id.bt_play_video_1) {
                    // Surface传递到Native函数中，用于绘制
                    result = mPlayer.render(videoFile.getAbsolutePath(), mSurface);
                } else if (id == R.id.bt_play_video_2) {
                    // 第二种方式，不使用libyuv
                    result = mPlayer.play(videoFile.getAbsolutePath(), mSurface);
                } else {
                    result = -1;
                    Log.e(TAG, "start: id error");
                }
                complete(result);
            }
        }).start();
    }
    ```

2. Native方法

    ```java
    public class MyPlayer {
    
        /**
         * 使用libyuv将YUV转换为RGB，进行播放
         * 部分格式播放时，会出现花屏
         *
         * @param input   输入视频路径
         * @param surface {@link android.view.Surface}
         * @return 0成功，-1失败
         */
        public native int render(String input, Surface surface);
    
        /**
         * 使用ffmpeg自带的swscale.h中的sws_scale将解码数据转换为RGB，进行播放
         * 不会出现花屏
         *
         * @param input   输入视频路径
         * @param surface {@link android.view.Surface}
         * @return 0成功，-1失败
         */
        public native int play(String input, Surface surface);
    
        /**
         * 停止播放
         */
        public native void stop();
    
        static {
            System.loadLibrary("player");
        }
    }
    ```

3. 开始编写C代码

    1. 需要引入头文件
        ```c
        // 需要引入native绘制的头文件
        #include <android/native_window_jni.h>
        #include <android/native_window.h>
        ```

    2. 在`native_window_jni.h`中可以发现关键的函数`ANativeWindow* ANativeWindow_fromSurface(JNIEnv* env, jobject surface);`
        ```c
        // 获取一个关联Surface的NativeWindow窗体
        ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
        ```

    3. 设置缓冲区的属性
        ```c
        // 设置缓冲区的属性（宽、高、像素格式），像素格式要和SurfaceView的像素格式一直
        ANativeWindow_setBuffersGeometry(nativeWindow, pCodecCtx->width, pCodecCtx->height,
                                         WINDOW_FORMAT_RGBA_8888);
        ```

    4. lock锁定下一个即将要绘制的Surface
        ```c
        ANativeWindow_lock(nativeWindow, &outBuffer, NULL);
        ```

    5. 格式转换，两种方式
    
        * 使用`libyuv`
        
            ```c
            // 读取帧画面放入缓冲区，指定RGB的AVFrame的像素格式、宽高和缓冲区
            avpicture_fill((AVPicture *) pRGBFrame,
                          outBuffer.bits,// 转换RGB的缓冲区，就使用绘制时的缓冲区，即可完成Surface绘制
                          AV_PIX_FMT_RGBA,// 像素格式
                          pCodecCtx->width, pCodecCtx->height);
            
            // 将YUV420P->RGBA_8888
            I420ToARGB(pFrame->data[0], pFrame->linesize[0],// Y
                      pFrame->data[2], pFrame->linesize[2],// V
                      pFrame->data[1], pFrame->linesize[1],// U
                      pRGBFrame->data[0], pRGBFrame->linesize[0],
                      pCodecCtx->width, pCodecCtx->height);
            ```
            
        * 使用`sws_scale()`
        
            1. 创建SwsContext
            
                ```c
                // av_image_get_buffer_size代替过时的avpicture_get_size
                int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, videoWidth, videoHeight, 1);
                // 缓冲区分配内存
                uint8_t *buffer = av_malloc(numBytes * sizeof(uint8_t));
                // 初始化缓冲区，av_image_fill_arrays替代过时的avpicture_fill
                av_image_fill_arrays(pRGBFrame->data, pRGBFrame->linesize, buffer, AV_PIX_FMT_RGBA,
                                     videoWidth, videoHeight, 1);
            
                // 由于解码出来的帧格式不是RGBA的，在渲染之前需要进行格式转换
                struct SwsContext *sws_ctx = sws_getContext(videoWidth, videoHeight,// 原始画面宽高
                                                            pCodecCtx->pix_fmt,// 原始画面像素格式
                                                            videoWidth, videoHeight,// 目标画面宽高
                                                            AV_PIX_FMT_RGBA,// 目标画面像素格式
                                                            SWS_BILINEAR,// 算法
                                                            NULL, NULL, NULL);
                ```
            
            2. 格式转换
            
                ```c
                sws_scale(sws_ctx, (uint8_t const *const *) pFrame->data,
                          pFrame->linesize, 0, videoHeight,
                          pRGBFrame->data, pRGBFrame->linesize);
                ```
            
            3. 逐行复制
            
                ```c
                // 获取stride
                uint8_t *dst = windowBuffer.bits;
                int dstStride = windowBuffer.stride * 4;
                uint8_t *src = pRGBFrame->data[0];
                int srcStride = pRGBFrame->linesize[0];

                // 由于window的stride和帧的stride不同，因此需要逐行复制
                int h;
                for (h = 0; h < videoHeight; h++) {
                    memcpy(dst + h * dstStride, src + h * srcStride, srcStride);
                }
                ```

    6. unlock绘制
       ```c
       ANativeWindow_unlockAndPost(nativeWindow);
       ```

    7. 释放资源
       ```c
       ANativeWindow_release(nativeWindow);
       ```

### 引入libyuv

1. 下载libyuv，google推出的，下载地址：https://chromium.googlesource.com/external/libyuv
使用git克隆到本次`git clone https://chromium.googlesource.com/external/libyuv`

2. 下载完成后在libyuv文件夹下新建jni文件夹，并将其他文件放置到jni目录下，因为要使用NDK编译，需要有jni目录

3. 打开`Android.mk`，找到最后三行修改为动态库
    ```
    LOCAL_MODULE := libyuv_static
    LOCAL_MODULE_TAGS := optional
    include $(BUILD_STATIC_LIBRARY)
    ```
    修改为
    ```
    LOCAL_MODULE := libyuv
    LOCAL_MODULE_TAGS := optional
    include $(BUILD_SHARED_LIBRARY)
    ```

4. 将libyuv文件夹上传到Linux，NDK的工程需要有jni的目录，并且有`Android.mk`的文件

5. 进行编译，需要在libyuv目录下，与jni同级的目录进行`ndk-build`
    ```
    root@FF-VM:/usr/jamff/libyuv# ls
    jni
    root@FF-VM:/usr/jamff/libyuv# ndk-build
    ```

6. 编译完成后，得到libs，引入到项目
    ```
    root@FF-VM:/usr/jamff/libyuv# ls
    jni libs obj
    root@FF-VM:/usr/jamff/libyuv# cd libs/
    root@FF-VM:/usr/jamff/libyuv/libs# ls
    arm64-v8a armeabi-v7a x86 x86_64
    ```

7. 引入libyuv头文件，并在cmake中配置

### 编译

此时编译发现找不到`android/native_window_jni.h`和`android/native_window.h`中的函数
需要在cmake的`target_link_libraries`中链接`libandroid.so`

```cmake
# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# 设置生成的so动态库最后输出的路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/jniLibs/${ANDROID_ABI})

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/main/jni/include)
# 设置头文件路径，在代码中include时不需要带路径了
include_directories(${INCLUDE_DIR}/ffmpeg ${INCLUDE_DIR}/libyuv)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             player

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             src/main/jni/player.c )

set(LIB_DIR ${CMAKE_SOURCE_DIR}/src/main/jniLibs)

add_library(avcodec SHARED IMPORTED)
# 设置导入的路径
set_target_properties(avcodec
                      PROPERTIES IMPORTED_LOCATION
                      ${LIB_DIR}/${ANDROID_ABI}/libavcodec.so)

add_library(avdevice SHARED IMPORTED)
set_target_properties(avdevice
                      PROPERTIES IMPORTED_LOCATION
                      ${LIB_DIR}/${ANDROID_ABI}/libavdevice.so)

add_library(avfilter SHARED IMPORTED)
set_target_properties(avfilter
                      PROPERTIES IMPORTED_LOCATION
                      ${LIB_DIR}/${ANDROID_ABI}/libavfilter.so)

add_library(avformat SHARED IMPORTED)
set_target_properties(avformat
                      PROPERTIES IMPORTED_LOCATION
                      ${LIB_DIR}/${ANDROID_ABI}/libavformat.so)

add_library(avutil SHARED IMPORTED)
set_target_properties(avutil
                      PROPERTIES IMPORTED_LOCATION
                      ${LIB_DIR}/${ANDROID_ABI}/libavutil.so)

add_library(postproc SHARED IMPORTED)
set_target_properties(postproc
                      PROPERTIES IMPORTED_LOCATION
                      ${LIB_DIR}/${ANDROID_ABI}/libpostproc.so)

add_library(swresample SHARED IMPORTED)
set_target_properties(swresample
                      PROPERTIES IMPORTED_LOCATION
                      ${LIB_DIR}/${ANDROID_ABI}/libswresample.so)

add_library(swscale SHARED IMPORTED)
set_target_properties(swscale
                      PROPERTIES IMPORTED_LOCATION
                      ${LIB_DIR}/${ANDROID_ABI}/libswscale.so)

add_library(yuv SHARED IMPORTED)
set_target_properties(yuv
                      PROPERTIES IMPORTED_LOCATION
                      ${LIB_DIR}/${ANDROID_ABI}/libyuv.so)

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       player
                       avcodec
                       avdevice
                       avfilter
                       avformat
                       avutil
                       postproc
                       swresample
                       swscale
                       yuv

                       # Links the target library to the log library
                       # included in the NDK.
                       # 使用native_window需要引入，android这里是简写，也可以仿照log-lib去写
                       android
                       ${log-lib} )
```

### 思路整理

1. SurfaceView创建完成时，打开文件，打开线程
2. NDK中初始化，通过ANativeWindow绘制
3. 得到ANativeWindow
4. 设置RGB缓冲区
5. ANativeWindow_lock
6. 给缓冲区赋值，将YUV转为RGB
7. ANativeWindow_unlock
8. 释放ANativeWindow资源

### 注意

* 播放偏慢问题
    
    如果不进行休眠，会导致解码一帧，显示一帧，播放过快，所以根据帧率设置休眠时间，但是由于解码也需要时间，这里播放就会偏慢一点

    ```c
    // 视频帧率，每秒多少帧
    double frame_rate = av_q2d(stream->avg_frame_rate);
    // 间隔是微秒
    int sleep = (int) (1000 * 1000 / frame_rate);
    ```
   
* 调用`render()`播放，部分资源会花屏，应该是使用libyuv转换RGB的有问题，使用`play()`播放没有问题

* 没有同步解码音频，播放无声

* 没有进行比例缩放，填充满屏幕

* 播放停止后，停留在最后一帧，需要手动清除SurfaceView画布，才可保证视频停止后，界面恢复初始状态

### 参考

[Android+FFmpeg+ANativeWindow视频解码播放](https://blog.csdn.net/glouds/article/details/50937266)  
[FFmpeg - time_base,r_frame_rate](https://blog.csdn.net/biezhihua/article/details/62260498)  
[FFMPEG结构体分析：AVStream](https://blog.csdn.net/leixiaohua1020/article/details/14215821)  
[ffmpeg time_base](http://www.cnitblog.com/luofuchong/archive/2014/11/28/89869.html)  
[ffmpeg中的时间](https://www.cnblogs.com/yinxiangpei/articles/3892982.html)  
[最简单的基于FFmpeg的libswscale的示例（YUV转RGB）](https://blog.csdn.net/leixiaohua1020/article/details/42134965)  
[SurfaceView清空画布的解决方案](https://blog.csdn.net/zhangfengwu2014/article/details/78126241)  

## 音频播放

播放音频解码后的PCM格式

* 使用C/C++播放——OpenSL ES
* 使用Java播放——AudioTrack

这里使用AudioTrack进行播放

### Native方法

```java
/**
 * 播放媒体中的音频
 *
 * @param input  输入媒体路径
 * @param output 输出音频PCM路径
 */
public native int playMusic(String input, String output);
```

### 创建AudioTrack

```java
/**
 * 创建AudioTrack，提供给C调用
 *
 * @param sampleRateInHz 采样率
 * @param nb_channels    声道数
 * @return {@link android.media.AudioTrack}
 */
private AudioTrack createAudioTrack(int sampleRateInHz, int nb_channels) {

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
```

### JNI调用

1. 调用`MyPlayer.createAudioTrack()`
    ```c
    // 获取MyPlayer的jclass
    jclass player_class = (*env)->GetObjectClass(env, instance);

    // 得到MyPlayer.createAudioTrack()
    jmethodID create_audio_track_mid = (*env)->GetMethodID(env, player_class,// jclass
                                                           "createAudioTrack",// 方法名
                                                           "(II)Landroid/media/AudioTrack;");// 字段签名

    // 调用MyPlayer.createAudioTrack()，创建AudioTrack实例
    jobject audio_track = (*env)->CallObjectMethod(env, instance,// jobject
                                                   create_audio_track_mid,// createAudioTrack()
                                                   out_sample_rate,// createAudioTrack的参数
                                                   out_channel_nb);// createAudioTrack的参数
    ```

2. 调用`AudioTrack.play()`

    ```c
    // 获取AudioTrack的jclass
    jclass audio_track_class = (*env)->GetObjectClass(env, audio_track);

    // 得到AudioTrack.play()
    jmethodID audio_track_play_mid = (*env)->GetMethodID(env, audio_track_class, "play", "()V");

    // 调用AudioTrack.play()
    (*env)->CallVoidMethod(env, audio_track, audio_track_play_mid);
    ```

3. 其他`AudioTrack`方法

    ```c
    // 得到AudioTrack.write()
    jmethodID audio_track_write_mid = (*env)->GetMethodID(env, audio_track_class,
                                                          "write", "([BII)I");
    
    // 得到AudioTrack.stop()
    jmethodID audio_track_stop_mid = (*env)->GetMethodID(env, audio_track_class, "stop", "()V");
    
    // 得到AudioTrack.release()
    jmethodID audio_track_release_mid = (*env)->GetMethodID(env, audio_track_class,
                                                            "release", "()V");
    ```

4. 调用`AudioTrack.write()`

    1. `out_buffer`缓冲区数据，转成`byte`数组
    
        ```c
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
        ```
    
    2. 调用`AudioTrack.write()`

        ```c
        (*env)->CallIntMethod(env, audio_track, audio_track_write_mid,
                              audio_sample_array,// 需要播放的数据数组
                              0, out_buffer_size);
        ```
    
    3. 释放局部资源
    
        ```c
        (*env)->DeleteLocalRef(env, audio_sample_array);
        ```

        这里如果不进行释放，运行一段时间会崩溃，内存溢出
        
        `JNI ERROR (app bug): local reference table overflow (max=512)`

4. 播放结束时调用`AudioTrack.stop()`和`AudioTrack.release()`

```c
// 调用AudioTrack.stop()
(*env)->CallVoidMethod(env, audio_track, audio_track_stop_mid);

// 调用AudioTrack.release()
(*env)->CallVoidMethod(env, audio_track, audio_track_release_mid);
```

### 如何得到字段签名

1. 要找到`.class`文件目录

    Android Studio的目录在`ffmpeg4_so\build\intermediates\classes`目录下

2. 进入`ffmpeg4_so\build\intermediates\classes\debug`打开命令行

3. 输入`javap -s -p 完整类名`就可得到字段签名

    ```
    javap -s -p com.jamff.ffmpeg.MyPlayer
    Compiled from "MyPlayer.java"
    public class com.jamff.ffmpeg.MyPlayer {
      private android.media.AudioTrack mAudioTrack;
        descriptor: Landroid/media/AudioTrack;
      public com.jamff.ffmpeg.MyPlayer();
        descriptor: ()V
    
      public native void render(java.lang.String, android.view.Surface);
        descriptor: (Ljava/lang/String;Landroid/view/Surface;)V
    
      public native int play(java.lang.String, android.view.Surface);
        descriptor: (Ljava/lang/String;Landroid/view/Surface;)I
    
      public native int playMusic(java.lang.String, java.lang.String);
        descriptor: (Ljava/lang/String;Ljava/lang/String;)I
    
      private android.media.AudioTrack createAudioTrack(int, int);
        descriptor: (II)Landroid/media/AudioTrack;
    
      public native void stop();
        descriptor: ()V
    
      static {};
        descriptor: ()V
    }
    ```

### 参考

[AudioTrack](https://developer.android.google.cn/reference/android/media/AudioTrack)  
[memcpy函数详解](https://blog.csdn.net/xiaominkong123/article/details/51733528/)  
[JNI内存泄露处理方法汇总](https://blog.csdn.net/wangpingfang/article/details/53945479)  
