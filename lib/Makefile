MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: all clean

all: libfilter.so libfilter.a

# TODO: without c11 compiled in, aligned_alloc is never used. But if
# it is compiled in, how can systems without a libc that cupposts
# aligned alloc work?
#
# Same with gnu and mmap, although probably header, object, and
# program all have the same answer to that one. Still need a way to test

INCLUDES=-I../include -I../pcg-c/include
WARN=-W -Wall -Wextra
RELEASE=-fPIC -O3
CFLAGS=-std=gnu11 $(INCLUDES) $(WARN) $(RELEASE)

%.d: %.c Makefile
	$(CC) $(CFLAGS) -MM $*.c -o new-$*.d
	if ! diff -q $*.d new-$*.d 2>/dev/null; then cp new-$*.d $*.d; fi

include util.d
include memory.d

%.o: %.c Makefile
	$(CC) $(CFLAGS) $*.c -o $*.o -c

libfilter.so: util.o memory.o Makefile
	$(CC) -fPIC -shared -o libfilter.so util.o memory.o ../pcg-c/src/libpcg_random.a

libfilter.a: util.o memory.o Makefile
	ar rcs libfilter.a util.o memory.o ../pcg-c/src/libpcg_random.a

clean:
	rm -f libfilter.so libfilter.a memory.o util.o
