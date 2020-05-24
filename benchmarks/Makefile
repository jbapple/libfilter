MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: all clean

all: bench

bench: bench.cpp ../include/simd-block.hpp ../include/simd-block.h ../include/memory.h ../include/util.h ../cpp/util.hpp ../pcg-c/include/pcg_variants.h ../lib/libfilter.a Makefile
	$(CXX) -std=c++17 -DNDEBUG -march=native -mtune=native -W -Wall -Wextra -I../include -I../pcg-c/include -I../cpp -O3 -ggdb3 bench.cpp ../lib/libfilter.a -o bench -lm

clean:
	rm -f bench
