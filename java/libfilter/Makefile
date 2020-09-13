.PHONY: default clean

default: libjni-bridge.so

include ../../c/common.mk

WARN=-W -Wall -Wextra -Werror -Wno-unused-parameter
RELEASE=-fPIC -O3 -ggdb3
INCLUDES=-I ../../c/include -I ${JAVA_HOME}/include -I ${JAVA_HOME}/include/linux
CFLAGS=-std=c99 -march=native -mtune=native $(INCLUDES) $(WARN) $(RELEASE)
LINKS=

include Library.d

libjni-bridge.so: Library.o Makefile
	$(CC) -fPIC -shared -o libjni-bridge.so Library.o libfilter.so -Wl,-rpath=$$ORIGIN

clean: Makefile
	rm -f Library.o libjni-bridge.so Library.d
