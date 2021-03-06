package com.jamff.ffmpeg;

import android.Manifest;
import android.content.pm.PackageManager;
import android.graphics.Canvas;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import com.jamff.ffmpeg.widget.VideoView;

import java.io.File;

public class MainActivity extends AppCompatActivity implements View.OnClickListener,
        SurfaceHolder.Callback, MyPlayer.OnCompletionListener {

    private static final String TAG = "JamFF";

    private static final String VIDEO_INPUT = "input.mp4";

    private static final int REQUEST_EXTERNAL_STORAGE = 1;
    private static String[] PERMISSIONS_STORAGE = {
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE};

    private VideoView video_view;
    private Button bt_play_video_1;
    private Button bt_play_video_2;
    private Button bt_play_music;
    private Button bt_stop;

    private MyPlayer mPlayer;
    private File videoFile;
    private Surface mSurface;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        initView();
        initData();
        initEvent();
    }

    private void initView() {
        video_view = findViewById(R.id.video_view);
        bt_play_video_1 = findViewById(R.id.bt_play_video_1);
        bt_play_video_2 = findViewById(R.id.bt_play_video_2);
        bt_play_music = findViewById(R.id.bt_play_music);
        bt_stop = findViewById(R.id.bt_stop);
    }

    private void initData() {
        checkPermission();
        checkVideoFile();
        mPlayer = new MyPlayer();
        mPlayer.setOnCompletionListener(this);
    }

    private void initEvent() {
        video_view.getHolder().addCallback(this);
        bt_play_video_1.setOnClickListener(this);
        bt_play_video_2.setOnClickListener(this);
        bt_play_music.setOnClickListener(this);
        bt_stop.setOnClickListener(this);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        // video_view.getHolder()和holder是一个对象
        mSurface = holder.getSurface();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.bt_play_video_1:
                playVideo(true);
                break;
            case R.id.bt_play_video_2:
                playVideo(false);
                break;
            case R.id.bt_play_music:
                playMusic();
                break;
            case R.id.bt_stop:
                mPlayer.stop();
                break;
            default:
                break;
        }
    }

    /**
     * 检查视频文件
     */
    private boolean checkVideoFile() {
        videoFile = new File(Environment.getExternalStorageDirectory(), VIDEO_INPUT);

        boolean exist = videoFile.exists();
        bt_play_video_1.setEnabled(exist);
        bt_play_video_2.setEnabled(exist);

        if (!exist) {
            Toast.makeText(this, videoFile.getAbsolutePath() + "文件不存在", Toast.LENGTH_SHORT).show();
        }

        return exist;
    }

    /**
     * 播放媒体文件的视频
     *
     * @param is_libyuv true 使用libyuv将YUV转换为RGB
     *                  false 使用ffmpeg自带的swscale.h中的sws_scale将解码数据转换为RGB
     */
    private void playVideo(boolean is_libyuv) {

        if (!checkVideoFile()) {
            return;
        }

        bt_play_video_1.setEnabled(false);
        bt_play_video_2.setEnabled(false);
        bt_play_music.setEnabled(false);
        bt_stop.setEnabled(true);

        if (mSurface == null) {
            Log.e(TAG, "start: mSurface == null");
            return;
        }

        // 子线程播放，播放完成后回调onCompletion
        if (is_libyuv) {
            mPlayer.renderVideo(videoFile.getAbsolutePath(), mSurface);
        } else {
            mPlayer.playVideo(videoFile.getAbsolutePath(), mSurface);
        }
    }

    /**
     * 播放媒体文件的音频
     */
    private void playMusic() {
        if (!checkVideoFile()) {
            return;
        }

        bt_play_video_1.setEnabled(false);
        bt_play_video_2.setEnabled(false);
        bt_play_music.setEnabled(false);
        bt_stop.setEnabled(true);

        if (mSurface == null) {
            Log.e(TAG, "start: mSurface == null");
            return;
        }

        // 子线程播放，播放完成后回调onCompletion
        mPlayer.playMusic(videoFile.getAbsolutePath(), mSurface);
    }

    @Override
    public void onCompletion(int result) {
        complete(result);
    }

    /**
     * 视频播放结束后，SurfaceView清空画布
     */
    public void clearDraw() {
        Canvas canvas = null;
        try {
            canvas = video_view.getHolder().lockCanvas(null);
            canvas.drawColor(Color.BLACK);
        } catch (Exception e) {
            // TODO: handle exception
        } finally {
            if (canvas != null) {
                video_view.getHolder().unlockCanvasAndPost(canvas);
            }
        }
    }

    /**
     * 播放完成，或者手动停止
     *
     * @param result 0成功，-1失败
     */
    private void complete(final int result) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                bt_play_video_1.setEnabled(true);
                bt_play_video_2.setEnabled(true);
                bt_play_music.setEnabled(true);
                clearDraw();
                if (result == 0) {
                    Toast.makeText(MainActivity.this, "播放完成", Toast.LENGTH_SHORT).show();
                } else {
                    Toast.makeText(MainActivity.this, "播放失败", Toast.LENGTH_SHORT).show();
                }
            }
        });
    }

    /**
     * 动态权限
     */
    private void checkPermission() {
        int permission = ActivityCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE);
        if (permission != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, PERMISSIONS_STORAGE, REQUEST_EXTERNAL_STORAGE);
            Log.d(TAG, "checkPermission: 尚未授权");
        } else {
            bt_play_video_1.setEnabled(true);
            bt_play_video_2.setEnabled(true);
            bt_play_music.setEnabled(true);
            Log.d(TAG, "checkPermission: 已经授权");
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_EXTERNAL_STORAGE) {
            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                bt_play_video_1.setEnabled(true);
                bt_play_video_2.setEnabled(true);
                bt_play_music.setEnabled(true);
                Log.d(TAG, "onRequestPermissionsResult: 接受权限");
            } else {
                Toast.makeText(this, "未开通文件读写权限", Toast.LENGTH_SHORT).show();
                if (ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                    Log.d(TAG, "onRequestPermissionsResult: 拒绝权限，再次进入应用还会提示");
                } else {
                    Log.d(TAG, "onRequestPermissionsResult: 拒绝权限，再次进入应用不再提示");
                }
            }
        }
    }

    @Override
    public void onBackPressed() {
        mPlayer.setOnCompletionListener(null);
        mPlayer.stop();
        super.onBackPressed();
    }

    @Override
    protected void onDestroy() {
        mPlayer.destroy();
        super.onDestroy();
    }
}
