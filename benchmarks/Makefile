.PHONY: all clean

all: bench

bench: bench.cpp ../include/simd-block.hpp ../include/simd-block.h ../include/memory.h ../include/util.h ../cpp/util.hpp ../pcg-c/include/pcg_variants.h ../lib/libfilter.so Makefile
	$(CXX) -std=c++17 -march=native -mtune=native -W -Wall -Wextra -I../include -I../pcg-c/include -I../cpp -O3 bench.cpp -o bench -L../lib -lfilter -lm

clean:
	rm -f bench
