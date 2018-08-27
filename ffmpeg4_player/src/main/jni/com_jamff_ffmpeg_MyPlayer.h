/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_jamff_ffmpeg_MyPlayer */

#ifndef _Included_com_jamff_ffmpeg_MyPlayer
#define _Included_com_jamff_ffmpeg_MyPlayer
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_jamff_ffmpeg_MyPlayer
 * Method:    init
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_jamff_ffmpeg_MyPlayer_init
        (JNIEnv *, jobject);

/*
 * Class:     com_jamff_ffmpeg_MyPlayer
 * Method:    render
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)V
 */
JNIEXPORT jint JNICALL Java_com_jamff_ffmpeg_MyPlayer_render
        (JNIEnv *, jobject, jstring, jobject);

/*
 * Class:     com_jamff_ffmpeg_MyPlayer
 * Method:    play
 * Signature: (Ljava/lang/String;Ljava/lang/Object;)V
 */
JNIEXPORT jint JNICALL Java_com_jamff_ffmpeg_MyPlayer_play
        (JNIEnv *, jobject, jstring, jobject);

/*
 * Class:     com_jamff_ffmpeg_MyPlayer
 * Method:    stop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_jamff_ffmpeg_MyPlayer_stop
        (JNIEnv *, jobject);

/*
 * Class:     com_jamff_ffmpeg_MyPlayer
 * Method:    destroy
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_jamff_ffmpeg_MyPlayer_destroy
        (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif
