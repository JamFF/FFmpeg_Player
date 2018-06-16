# FFmpeg_Player
在NDK r17环境下，使用老脚本，编译FFmpeg4.0动态库，使用FFmpeg将MP4解码YUV

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