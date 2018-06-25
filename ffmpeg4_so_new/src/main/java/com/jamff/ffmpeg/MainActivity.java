package com.jamff.ffmpeg;

import android.os.Bundle;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.File;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private static final String INPUT = new File(Environment.getExternalStorageDirectory(), "input.mp4").getAbsolutePath();
    private static final String OUTPUT = new File(Environment.getExternalStorageDirectory(), "output_480x272_yuv420p.yuv").getAbsolutePath();

    private Button bt_decode;
    private TextView tv_version;
    private TextView tv_hint;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        initView();
        initData();
    }

    private void initView() {
        tv_version = findViewById(R.id.tv_version);
        tv_hint = findViewById(R.id.tv_hint);
        bt_decode = findViewById(R.id.bt_decode);
        bt_decode.setOnClickListener(this);
    }

    private void initData() {
        tv_version.setText(VideoUtils.version());

        File inputFile = new File(INPUT);
        File outputFile = new File(OUTPUT);
        if (inputFile.exists()) {
            if (outputFile.exists()) {
                bt_decode.setEnabled(false);
                tv_hint.setText("解码文件已存在");
                bt_decode.setText("解码完成");
            } else {
                bt_decode.setEnabled(true);
            }
        } else {
            tv_hint.setText(String.format("%s文件不存在", INPUT));
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
                        VideoUtils.decode(INPUT, OUTPUT);
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
}
