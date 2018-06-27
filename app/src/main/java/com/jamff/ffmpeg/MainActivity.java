package com.jamff.ffmpeg;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.File;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private static final String TAG = "JamFF";

    private static final String VIDEO_INPUT = "input_mini.mp4";// 分辨率是480x272
    private static final String VIDEO_OUTPUT = "output_480x272_yuv420p.yuv";// 根据输入格式的分辨率命名

    private static final String AUDIO_INPUT = "Love Story.mp3";
    private static final String AUDIO_OUTPUT = "Love Story.pcm";

    private static final int REQUEST_EXTERNAL_STORAGE = 1;
    private static String[] PERMISSIONS_STORAGE = {
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE};

    private File videoInputFile, videoOutputFile;
    private File audioInputFile, audioOutputFile;

    private Button bt_video_decode, bt_audio_decode;
    private TextView tv_video_hint, tv_audio_hint;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        initView();
        initData();
    }

    private void initView() {
        tv_video_hint = findViewById(R.id.tv_video_hint);
        tv_audio_hint = findViewById(R.id.tv_audio_hint);
        bt_video_decode = findViewById(R.id.bt_video_decode);
        bt_video_decode.setOnClickListener(this);
        bt_audio_decode = findViewById(R.id.bt_audio_decode);
        bt_audio_decode.setOnClickListener(this);
    }

    private void initData() {
        checkPermission();
        checkVideoFile();
        checkAudioFile();
    }

    /**
     * 动态权限
     */
    private void checkPermission() {
        int permission = ActivityCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE);
        if (permission != PackageManager.PERMISSION_GRANTED) {
            Log.d(TAG, "checkPermission: 尚未授权");
            ActivityCompat.requestPermissions(this, PERMISSIONS_STORAGE, REQUEST_EXTERNAL_STORAGE);
        } else {
            Log.d(TAG, "checkPermission: 已经授权");
        }
    }

    /**
     * 检查视频文件
     */
    private void checkVideoFile() {
        videoInputFile = new File(Environment.getExternalStorageDirectory(), VIDEO_INPUT);
        videoOutputFile = new File(Environment.getExternalStorageDirectory(), VIDEO_OUTPUT);

        if (videoInputFile.exists()) {
            if (videoOutputFile.exists()) {
                bt_video_decode.setEnabled(false);
                tv_video_hint.setText("视频解码文件已存在");
                bt_video_decode.setText("视频解码完成");
            } else {
                bt_video_decode.setEnabled(true);
            }
        } else {
            tv_video_hint.setText(String.format("%s文件不存在", VIDEO_INPUT));
        }
    }

    /**
     * 检查音频文件
     */
    private void checkAudioFile() {
        audioInputFile = new File(Environment.getExternalStorageDirectory(), AUDIO_INPUT);
        audioOutputFile = new File(Environment.getExternalStorageDirectory(), AUDIO_OUTPUT);

        if (audioInputFile.exists()) {
            if (audioOutputFile.exists()) {
                bt_audio_decode.setEnabled(false);
                tv_audio_hint.setText("音频解码文件已存在");
                bt_audio_decode.setText("音频解码完成");
            } else {
                bt_audio_decode.setEnabled(true);
            }
        } else {
            tv_audio_hint.setText(String.format("%s文件不存在", VIDEO_INPUT));
        }
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.bt_video_decode:
                videoDecode();
                break;
            case R.id.bt_audio_decode:
                audioDecode();
                break;
        }
    }

    /**
     * 视频解码
     */
    private void videoDecode() {
        bt_video_decode.setEnabled(false);
        bt_video_decode.setText("视频解码中");
        new Thread(new Runnable() {
            @Override
            public void run() {
                final int result = DecodeUtils.decodeVideo(videoInputFile.getAbsolutePath(), videoOutputFile.getAbsolutePath());

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (result == 0) {
                            bt_video_decode.setText("视频解码完成");
                        } else {
                            bt_video_decode.setText("视频解码失败");
                        }
                    }
                });
            }
        }).start();
    }

    /**
     * 音频解码
     */
    private void audioDecode() {
        bt_audio_decode.setEnabled(false);
        bt_audio_decode.setText("音频解码中");
        new Thread(new Runnable() {
            @Override
            public void run() {
                final int result = DecodeUtils.decodeAudio(audioInputFile.getAbsolutePath(), audioOutputFile.getAbsolutePath());

                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (result == 0) {
                            bt_audio_decode.setText("音频解码完成");
                        } else {
                            bt_audio_decode.setText("音频解码失败");
                        }
                    }
                });
            }
        }).start();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_EXTERNAL_STORAGE) {
            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Log.d(TAG, "onRequestPermissionsResult: 接受权限");
            } else {
                tv_audio_hint.setText("未开通文件读写权限");
                tv_video_hint.setText("未开通文件读写权限");
                bt_audio_decode.setEnabled(false);
                bt_video_decode.setEnabled(false);
                if (ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                    Log.d(TAG, "onRequestPermissionsResult: 拒绝权限，再次进入应用还会提示");
                } else {
                    Log.d(TAG, "onRequestPermissionsResult: 拒绝权限，再次进入应用不再提示");
                }
            }
        }
    }
}
