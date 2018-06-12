#include <jni.h>

// C++调C使用extern "C"
extern "C" {
#include <libavcodec/avcodec.h>
}

// C++调C使用extern "C"
extern "C" JNIEXPORT jstring

JNICALL
Java_com_jamff_ffmpeg_last_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    return env->NewStringUTF(av_version_info());
}