package com.jamff.ffmpeg;

import android.view.Surface;

/**
 * 描述：
 * 作者：JamFF
 * 创建时间：2018/6/21 14:50
 */
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
