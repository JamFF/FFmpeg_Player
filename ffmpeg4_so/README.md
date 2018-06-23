# FFmpeg_Player
在NDK r17环境下，使用老脚本，编译FFmpeg4.0动态库，使用FFmpeg将MP4解码YUV，转为RGB并绘制到UI

##### 编译so的脚本
```
#!/bin/bash

#一系列命令的集合 cd xx;dir

#ndkr17

#shell脚本第一行必须是指定shell脚本解释器

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
export ADDI_CFLAGS="-marm"

#给编译器的变量
#定义变量 值从FFmpeg_Player\app\.externalNativeBuild\cmake\release\armeabi-v7a\build.ninja >复制的
FLAG="-isystem $NDK/sysroot/usr/include/arm-linux-androideabi -D__ANDROID_API__=21 -g -DANDROID -ffunction-sections -funwind-tables -fstack-protector-strong -no-canonical-prefixes -march=armv7-a -mfloat-abi=softfp -mfpu=vfpv3-d16 -mthumb -Wa,--noexecstack -Wformat -Werror=for    mat-security  -Os -DNDEBUG  -fPIC"

INCLUDES="-isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/include -isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a/include -isystem $NDK/sources/cxx-stl/gnu-libstdc++/4.9/include/backward"

./configure \
--target-os=android \
--prefix=$PREFIX \
--arch=arm \
--disable-doc \
--enable-shared \
--disable-static \
--disable-yasm \
--disable-symver \
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

##### 思路整理
1. SurfaceView创建完成时，打开文件，打开线程
2. NDK中初始化，通过ANativeWindow绘制
3. 得到ANativeWindow
4. 设置RGB缓冲区
5. ANativeWindow_lock
6. 给缓冲区赋值，将YUV转为RGB
7. ANativeWindow_unlock
8. 释放ANativeWindow资源

##### 绘制UI

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
                android:id="@+id/bt_start_1"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:enabled="false"
                android:text="@string/start_1"/>

            <Button
                android:id="@+id/bt_start_2"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:enabled="false"
                android:text="@string/start_2"/>

            <Button
                android:id="@+id/bt_stop"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:enabled="false"
                android:text="@string/stop"/>

        </LinearLayout>

    </FrameLayout>
    ```

##### 绘制UI

1. 将文件路径以及Surface传入

    ```java
    private void play(final int id) {
        if (mSurface == null) {
            Log.e(TAG, "start: mSurface == null");
            return;
        }
        new Thread(new Runnable() {
            @Override
            public void run() {
                if (id == R.id.bt_start_1) {
                    // Surface传递到Native函数中，用于绘制
                    mPlayer.render(mFile.getAbsolutePath(), mSurface);
                } else if (id == R.id.bt_start_2) {
                    // 第二种方式，不使用libyuv
                    mPlayer.play(mFile.getAbsolutePath(), mSurface);
                } else {
                    Log.e(TAG, "start: id error");
                }
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
         */
        public native void render(String input, Surface surface);

        /**
         * 使用ffmpeg自带的swscale.h中的sws_scale将解码数据转换为RGB，进行播放
         * 不会出现花屏
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

    5. 写入缓冲区，这里需要下面引入libyuv
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

    6. unlock绘制
       ```c
       ANativeWindow_unlockAndPost(nativeWindow);
       ```

    7. 释放资源
       ```c
       ANativeWindow_release(nativeWindow);
       ```

##### 引入libyuv

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

4. 将libyuv文件夹上传到Linux上，NDK的工程需要有jni的目录，并且有`Android.mk`的文件

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

##### 编译

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
             src/main/jni/ffmpeg_player.c )

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

##### 注意

* 播放偏慢问题
    
    如果不进行休眠，会导致解码一帧，显示一帧，播放过快，所以根据帧率设置休眠时间，但是由于解码也需要时间，这里播放就会偏慢一点

    ```c
    // 视频帧率，每秒多少帧
    double frame_rate = av_q2d(stream->avg_frame_rate);
    // 间隔是微秒
    int sleep = (int) (1000 * 1000 / frame_rate);
    ```
   
* 调用`render`方法播放，部分资源会花屏，应该是libyuv使用的有问题，使用`play`方法播放没有问题

* 没有解码音频，播放无声

* 没有进行比例缩放，填充满屏幕


参考：
[Android+FFmpeg+ANativeWindow视频解码播放](https://blog.csdn.net/glouds/article/details/50937266)
[FFmpeg - time_base,r_frame_rate](https://blog.csdn.net/biezhihua/article/details/62260498)
[FFMPEG结构体分析：AVStream](https://blog.csdn.net/leixiaohua1020/article/details/14215821)