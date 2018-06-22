package com.jamff.ffmpeg;

import android.view.Surface;

/**
 * 描述：
 * 作者：JamFF
 * 创建时间：2018/6/21 14:50
 */
public class MyPlayer {

    public native void render(String input, Surface surface);

    public native void stop();

    static {
        System.loadLibrary("player");
    }
}
