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

    private static final String INPUT = "input_mini.mp4";// 分辨率是480x272
    private static final String OUTPUT = "output_480x272_yuv420p.yuv";// 根据输入格式的分辨率命名

    private static final int REQUEST_EXTERNAL_STORAGE = 1;
    private static String[] PERMISSIONS_STORAGE = {
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE};

    private File mInputFile;
    private File mOutputFile;

    private Button bt_decode;
    private TextView tv;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        initView();
        initData();
    }

    private void initView() {
        tv = findViewById(R.id.sample_text);
        bt_decode = findViewById(R.id.bt_decode);
        bt_decode.setOnClickListener(this);
    }

    private void initData() {

        checkPermission();

        mInputFile = new File(Environment.getExternalStorageDirectory(), INPUT);
        mOutputFile = new File(Environment.getExternalStorageDirectory(), OUTPUT);

        if (mInputFile.exists()) {
            if (mOutputFile.exists()) {
                bt_decode.setEnabled(false);
                tv.setText("解码文件已存在");
                bt_decode.setText("解码完成");
            } else {
                bt_decode.setEnabled(true);
            }
        } else {
            tv.setText(String.format("%s文件不存在", INPUT));
        }
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.bt_decode:
                bt_decode.setEnabled(false);
                bt_decode.setText("解码中");
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        VideoUtils.decode(mInputFile.getAbsolutePath(), mOutputFile.getAbsolutePath());
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                bt_decode.setText("解码完成");
                            }
                        });
                    }
                }).start();
                break;
        }
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

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_EXTERNAL_STORAGE) {
            if (grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Log.d(TAG, "onRequestPermissionsResult: 接受权限");
            } else {
                tv.setText("未开通文件读写权限");
                bt_decode.setEnabled(false);
                if (ActivityCompat.shouldShowRequestPermissionRationale(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
                    Log.d(TAG, "onRequestPermissionsResult: 拒绝权限，再次进入应用还会提示");
                } else {
                    Log.d(TAG, "onRequestPermissionsResult: 拒绝权限，再次进入应用不再提示");
                }
            }
        }
    }
}
