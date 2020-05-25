MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: all clean

all: block

block: block.cpp ../include/simd-block.hpp ../include/simd-block.h ../include/memory.h ../include/util.h ../lib/libfilter.a ../googletest/build/lib/libgtest_main.a ../googletest/build/lib/libgtest.a ../cpp/util.hpp Makefile
	$(CXX) -std=c++17 -march=native -mtune=native -O3 -W -Wall -Wextra -I../cpp -I../pcg-c/include -I../include -I../googletest/googletest/include -ggdb3 block.cpp ../googletest/build/lib/libgtest_main.a ../lib/libfilter.a ../googletest/build/lib/libgtest.a -o block -lm -lpthread

clean:
	rm -f block
