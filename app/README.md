# FFmpeg_Player
在NDK r10e环境下，使用老脚本，编译FFmpeg2.6.9动态库，使用FFmpeg将MP4解码YUV

```
#!/bin/bash
make clean
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