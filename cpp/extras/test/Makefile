.PHONY: default world clean googletest

default: block.exe paths.exe

world: default

clean:
	rm -f block.exe block.o block.d block.d.new
	rm -f paths.exe paths.o paths.d paths.d.new
	rm -rf googletest/build

LINKS += googletest/build/lib/libgtest_main.a googletest/build/lib/libgtest.a -lpthread
INCLUDES += -I googletest/googletest/include
export CXXFLAGS := -std=c++17 $(INCLUDES) $(WARN) $(RELEASE) -ggdb3 -O0

include $(DEFAULT_RECIPE)

include block.d
include paths.d

googletest/build/lib/libgtest_main.a googletest/build/lib/libgtest.a: googletest

block.exe: googletest/build/lib/libgtest_main.a googletest/build/lib/libgtest.a
paths.exe: googletest/build/lib/libgtest_main.a googletest/build/lib/libgtest.a

googletest: Makefile
	cd googletest && mkdir -p build && cd build && cmake -E env CXXFLAGS=--std=c++11 cmake ..
	$(MAKE) -C googletest/build
