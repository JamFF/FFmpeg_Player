package com.jamff.ffmpeg;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.Surface;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;

import com.jamff.ffmpeg.widget.VideoView;

import java.io.File;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private static final String TAG = "JamFF";

    private static final String INPUT = "input.mp4";

    private static final int REQUEST_EXTERNAL_STORAGE = 1;
    private static String[] PERMISSIONS_STORAGE = {
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE};

    private VideoView video_view;
    private Button bt_start;
    private Button bt_stop;

    private MyPlayer mPlayer;
    private File mFile;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        initView();
        initData();
    }

    private void initView() {
        video_view = findViewById(R.id.video_view);
        bt_start = findViewById(R.id.bt_start);
        bt_start.setOnClickListener(this);
        bt_stop = findViewById(R.id.bt_stop);
        bt_stop.setOnClickListener(this);
    }

    private void initData() {
        checkPermission();
        mPlayer = new MyPlayer();
        mFile = new File(Environment.getExternalStorageDirectory(), INPUT);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.bt_start:
                if (mFile.exists()) {
                    bt_stop.setEnabled(true);
                    start();
                } else {
                    Toast.makeText(this, mFile.getAbsolutePath() + "文件不存在", Toast.LENGTH_SHORT).show();
                }
                break;
            case R.id.bt_stop:
                mPlayer.stop();
                break;
        }
    }

    private void start() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                // Surface传递到Native函数中，用于绘制
                Surface surface = video_view.getHolder().getSurface();
                mPlayer.render(mFile.getAbsolutePath(), surface);
            }
        }).start();
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
            bt_start.setEnabled(true);
            Log.d(TAG, "checkPermission: 已经授权");
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_EXTERNAL_STORAGE) {
            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                bt_start.setEnabled(true);
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
}
