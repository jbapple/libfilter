#include <jni.h>

#include <stdio.h>

#include <filter/block.h>

JNIEXPORT
void JNICALL Java_Library_doNothing(JNIEnv* a, jobject b) {}

JNIEXPORT
jboolean JNICALL Java_Library_Allocate(JNIEnv* a, jobject obj, jobject bb, jint bytes) {
  // TODO: if the address is not aligned as we want, we probably need to memcpy
  // exit(1);
  // printf("uhoh\n");
  libfilter_block* me = (*a)->GetDirectBufferAddress(a, bb);
  return libfilter_block_init(bytes, me) >= 0;
}

JNIEXPORT jboolean JNICALL Java_Library_FindDetail(JNIEnv* a, jobject obj, jobject bb,
                                                   jlong hashval) {
  // return false;
  const libfilter_block* me = (*a)->GetDirectBufferAddress(a, bb);
  return libfilter_block_find_hash(hashval, me);
}

JNIEXPORT void JNICALL Java_Library_AddDetail(JNIEnv* a, jobject obj, jobject bb,
                                              jlong hashval) {
  libfilter_block* me = (*a)->GetDirectBufferAddress(a, bb);
  libfilter_block_add_hash(hashval, me);
}

JNIEXPORT jboolean JNICALL Java_Library_Deallocate
(JNIEnv * env, jobject obj, jobject bb) {
  libfilter_block* me = (*env)->GetDirectBufferAddress(env, bb);
  return libfilter_block_destruct(me) >= 0;
}
