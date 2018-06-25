package com.jamff.ffmpeg;

/**
 * 描述：
 * 作者：JamFF
 * 创建时间：2018/6/3 18:30
 */
public class VideoUtils {

    public native static void decode(String input, String output);

    public native static String version();

    static {
        System.loadLibrary("myffmpeg");
    }
}
