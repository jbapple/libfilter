.PHONY: all clean

all: c-example cpp-example

c-example: example.c ../include/simd-block.h ../include/memory.h ../include/util.h  ../lib/libfilter.so ../pcg-c/include/pcg_variants.h Makefile
	$(CC) -std=c99 -march=native -mtune=native -W -Wall -Wextra -I../include -O3 example.c -o c-example -L../lib -lfilter -lm

cpp-example: example.cpp ../include/simd-block.hpp ../include/simd-block.h ../include/memory.h ../include/util.h ../cpp/util.hpp ../pcg-c/include/pcg_variants.h ../lib/libfilter.so Makefile
	$(CXX) -std=c++11 -march=native -mtune=native -W -Wall -Wextra -I../include -I../pcg-c/include -I../cpp -O3 example.cpp -o cpp-example -L../lib -lfilter -lm

clean:
	rm -f c-example cpp-example
