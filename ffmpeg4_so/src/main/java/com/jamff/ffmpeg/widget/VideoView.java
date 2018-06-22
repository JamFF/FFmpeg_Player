package com.jamff.ffmpeg.widget;

import android.content.Context;
import android.graphics.PixelFormat;
import android.util.AttributeSet;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

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
