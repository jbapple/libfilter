MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: all clean

INCLUDES=-I../include -I../pcg-c/include -I../cpp
LINKS=../lib/libfilter.a -lm
WARN=-W -Wall -Wextra
RELEASE=-march=native -mtune=native -O3
CXXFLAGS=-std=c++11 $(INCLUDES) $(WARN) $(RELEASE)
CFLAGS=-std=c99 $(INCLUDES) $(WARN) $(RELEASE)

all: c-example cpp-example

clean:
	rm -f c-example cpp-example cpp-example.o cpp-example.d c-example.o c-example.d

############ C++ ###################

cpp-example.d: example.cpp Makefile
	$(CXX) $(CXXFLAGS) -MM example.cpp -o new-cpp-example.d
	if ! diff -q cpp-example.d new-cpp-example.d 2>/dev/null; then cp new-cpp-example.d cpp-example.d; fi

include cpp-example.d

cpp-example.o: example.cpp Makefile
	$(CXX) $(CXXFLAGS) example.cpp -o cpp-example.o -c

cpp-example: cpp-example.o ../lib/libfilter.a Makefile
	$(CXX) $(CXXFLAGS) cpp-example.o -o cpp-example $(LINKS)

############ C #####################

c-example.d: example.c Makefile
	$(CC) $(CFLAGS) -MM example.c -o new-c-example.d
	if ! diff -q c-example.d new-c-example.d; then cp new-c-example.d c-example.d; fi

include c-example.d

c-example.o: example.c Makefile
	$(CC) $(CFLAGS) example.c -o c-example.o -c

c-example: c-example.o ../lib/libfilter.a Makefile
	$(CC) $(CFLAGS) c-example.o -o c-example $(LINKS)
