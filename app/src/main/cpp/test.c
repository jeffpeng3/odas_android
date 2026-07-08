#include <jni.h>

JNIEXPORT jint JNICALL Java_com_example_myapplication_MainActivity_stringFromCJNI(
        JNIEnv* env,
        jobject o /* this */) {
    char* hello = "Fucking from C";
//    return (*env)->NewStringUTF(env, hello);
    return 1234;
}