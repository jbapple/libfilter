.PHONY: default clean

default: libjni-bridge.so

include ../../c/common.mk

WARN=-W -Wall -Wextra -Werror -Wno-unused-parameter
RELEASE=-fPIC -O3 -ggdb3
INCLUDES=-I ../../c/include -I ${JAVA_HOME}/include -I ${JAVA_HOME}/include/linux
CFLAGS=-std=c99 -march=native -mtune=native $(INCLUDES) $(WARN) $(RELEASE)
LINKS=

include jni-bridge.d

libjni-bridge.so: jni-bridge.o Makefile
	$(CC) -fPIC -shared -o libjni-bridge.so jni-bridge.o libfilter.so -Wl,-rpath=$$ORIGIN

clean: Makefile
	rm -f libjni-bridge.so jni-bridge.o jni-bridge.d jni-bridge.d.new
