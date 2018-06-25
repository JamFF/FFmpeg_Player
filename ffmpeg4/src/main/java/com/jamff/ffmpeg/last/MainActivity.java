package com.jamff.ffmpeg.last;

import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        TextView tv = findViewById(R.id.tv_version);
        tv.setText(stringFromJNI());
    }

    public native String stringFromJNI();

    static {
        System.loadLibrary("native-lib");
    }
}
