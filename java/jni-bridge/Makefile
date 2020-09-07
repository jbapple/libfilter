.PHONY: default clean

default: libjni-bridge.so

include common.mk

WARN=-W -Wall -Wextra -Werror -Wno-unused-parameter
RELEASE=-fPIC -O3 -ggdb3
INCLUDES=-I include -I ${JAVA_HOME}/include -I ${JAVA_HOME}/include/linux
CFLAGS=-std=gnu11 -march=native -mtune=native $(INCLUDES) $(WARN) $(RELEASE)
LINKS=

include Library.d

libjni-bridge.so: Library.o Makefile
	$(CC) -fPIC -shared -o libjni-bridge.so Library.o -Wl,-L$(CURDIR)/lib -lfilter -Wl,-rpath=$(CURDIR)/lib

clean: Makefile
	rm -f Library.o libjni-bridge.so
