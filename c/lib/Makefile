.PHONY: all lib

all: lib

include memory.d
include util.d
include block.d
include hash.d

lib: libfilter.a Makefile #libfilter.so Makefile

# TODO: without c11 compiled in, aligned_alloc is never used. But if
# it is compiled in, how can systems without a libc that cupposts
# aligned alloc work?
#
# Same with gnu and mmap, although probably header, object, and
# program all have the same answer to that one. Still need a way to test

WARN=-W -Wall -Wextra -Werror
RELEASE=-fPIC -O3 -ggdb3
CFLAGS=-std=gnu11 $(INCLUDES) $(WARN) $(RELEASE) #-march=native -mtune=native $(INCLUDES) $(WARN) $(RELEASE)
LINKS=

include $(DEFAULT_RECIPE)

libfilter.so: util.o memory.o block.o hash.o Makefile
	$(CC) -fPIC -shared -o libfilter.so util.o memory.o block.o hash.o

libfilter.a: util.o memory.o block.o hash.o Makefile
	ar rcs libfilter.a util.o memory.o block.o hash.o

clean:
	rm -f libfilter.so libfilter.a
	rm -f memory.o util.o block.o hash.o
	rm -f util.d.new memory.d.new block.d.new hash.d.new
	rm -f util.d memory.d block.d hash.d
