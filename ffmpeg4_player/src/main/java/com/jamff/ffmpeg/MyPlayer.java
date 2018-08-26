package com.jamff.ffmpeg;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;

/**
 * 描述：
 * 作者：JamFF
 * 创建时间：2018/6/21 14:50
 */
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
     * @param input   输入视频路径
     * @param surface {@link android.view.Surface}
     */
    public native void render(String input, Object surface);

    /**
     * 使用ffmpeg自带的swscale.h中的sws_scale将解码数据转换为RGB，进行播放
     * 不会出现花屏
     *
     * @param input   输入视频路径
     * @param surface {@link android.view.Surface}
     */
    public native void play(String input, Object surface);

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
     * @param nb_channels    声道数
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
