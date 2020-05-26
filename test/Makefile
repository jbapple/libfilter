MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: all clean

all: block

INCLUDES=-I../include -I../pcg-c/include -I../cpp -I../googletest/googletest/include
LINKS=../lib/libfilter.a ../googletest/build/lib/libgtest_main.a ../googletest/build/lib/libgtest.a -lm -lpthread
WARN=-W -Wall -Wextra
RELEASE=-march=native -mtune=native -O3
CXXFLAGS=-std=c++17 $(INCLUDES) $(WARN) $(RELEASE)

block.d: block.cpp Makefile
	$(CXX) $(CXXFLAGS) -MM block.cpp -o new-block.d
	if ! diff -q block.d new-block.d 2>/dev/null; then cp new-block.d block.d; fi

include block.d

block.o: block.cpp Makefile
	$(CXX) $(CXXFLAGS) block.cpp -o block.o -c

block: block.o ../lib/libfilter.a Makefile
	$(CXX) $(CXXFLAGS) block.o -o block $(LINKS)

clean:
	rm -f block block.d block.o
