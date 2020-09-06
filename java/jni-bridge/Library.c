// { pushd src/main/java/ && javac -h . Library.java && clang-9 -c -fPIC -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux -I ../../../../c/include Library.c -o Library.o && clang-9 -shared -fPIC -o libnative.so Library.o -lc ; popd ; } && ./gradlew check


// #include "Library.h"
#include <jni.h>

#include <filter/block.h>

JNIEXPORT
void JNICALL Java_Library_doNothing(JNIEnv* a, jobject b) {}

JNIEXPORT
jboolean JNICALL Java_Library_Allocate(JNIEnv* a, jobject obj, jobject bb, jint bytes) {
  libfilter_block* me = (*a)->GetDirectBufferAddress(a, bb);
  return libfilter_block_init(bytes, me) >= 0;
}

JNIEXPORT jboolean JNICALL Java_Library_FindDetail(JNIEnv* a, jobject obj, jobject bb,
                                                   jlong hashval) {
  const libfilter_block* me = (*a)->GetDirectBufferAddress(a, bb);
  return libfilter_block_find_hash(hashval, me);
}

JNIEXPORT void JNICALL Java_Library_AddDetail(JNIEnv* a, jobject obj, jobject bb,
                                              jlong hashval) {
  libfilter_block* me = (*a)->GetDirectBufferAddress(a, bb);
  printf("0x%lx\n",(unsigned long)me);
  libfilter_block_add_hash(hashval, me);
}

JNIEXPORT jboolean JNICALL Java_Library_Deallocate
(JNIEnv * env, jobject obj, jobject bb) {
  libfilter_block* me = (*env)->GetDirectBufferAddress(env, bb);
  return libfilter_block_destruct(me) >= 0;
}


//  private native boolean Find(ByteBuffer bb, long hashval);
