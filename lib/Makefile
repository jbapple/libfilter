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

util.o: util.c ../include/util.h ../include/memory.h ../pcg-c/include/pcg_variants.h Makefile
	$(CC) -std=gnu11 -O3 -fPIC -I../include -I../pcg-c/include/ util.c -c -o util.o

memory.o: memory.c ../include/memory.h Makefile
	$(CC) -std=gnu11 -O3 -fPIC -I../include memory.c -c -o memory.o

libfilter.so: util.o memory.o Makefile
	$(CC) -fPIC -shared -o libfilter.so util.o memory.o ../pcg-c/src/libpcg_random.a

libfilter.a: util.o memory.o Makefile
	ar rcs libfilter.a util.o memory.o ../pcg-c/src/libpcg_random.a

clean:
	rm -f libfilter.so libfilter.a memory.o util.o
