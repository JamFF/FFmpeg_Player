package com.jamff.ffmpeg;

/**
 * 描述：解码工具类
 * 作者：JamFF
 * 创建时间：2018/6/3 18:30
 */
public class DecodeUtils {

    /**
     * 音频解码
     *
     * @param input  输入音频路径
     * @param output 解码后PCM保存路径
     * @return 0成功，-1失败
     */
    public native static int decodeAudio(String input, String output);

    /**
     * 视频解码
     *
     * @param input  输入视频路径
     * @param output 解码后YUV保存路径
     * @return 0成功，-1失败
     */
    public native static int decodeVideo(String input, String output);

    static {
        System.loadLibrary("myffmpeg");
    }
}
