MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: all clean

all: bench

INCLUDES=-I../include -I../pcg-c/include -I../cpp
LINKS=../lib/libfilter.a -lm
WARN=-W -Wall -Wextra
RELEASE=-march=native -mtune=native -O3
CXXFLAGS=-std=c++17 $(INCLUDES) $(WARN) $(RELEASE)

bench.d: bench.cpp Makefile
	$(CXX) $(CXXFLAGS) -MM bench.cpp -o new-bench.d
	if ! diff -q bench.d new-bench.d 2>/dev/null; then cp new-bench.d bench.d; fi

include bench.d

bench.o: bench.cpp Makefile
	$(CXX) $(CXXFLAGS) bench.cpp -o bench.o -c

bench: bench.o ../lib/libfilter.a Makefile
	$(CXX) $(CXXFLAGS) bench.o -o bench $(LINKS)

clean:
	rm -f bench bench.d bench.o
