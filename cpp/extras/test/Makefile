.PHONY: default world clean googletest

default: block.exe

world: default

clean:
	rm -f block.exe block.o block.d block.d.new
	rm -rf googletest/build

LINKS += googletest/build/lib/libgtest_main.a googletest/build/lib/libgtest.a -lpthread -L${JAVA_HOME}/jre/lib/amd64/server -L${JAVA_HOME}/lib/server -L ${JAVA_HOME}/jre/lib/server/ -ljvm
INCLUDES += -I googletest/googletest/include -I ${JAVA_HOME}/include -I ${JAVA_HOME}/include/linux -I ${JAVA_HOME}/include/win32 -I ${JAVA_HOME}/include/darwin
export CXXFLAGS := -std=c++17 $(INCLUDES) $(WARN) $(RELEASE) -ggdb3 # -UNDEBUG  -O0 -march=x86-64 -fsanitize=undefined #-fsanitize=address

include $(DEFAULT_RECIPE)

include block.d

googletest/build/lib/libgtest_main.a googletest/build/lib/libgtest.a: googletest

block.exe: googletest/build/lib/libgtest_main.a googletest/build/lib/libgtest.a

googletest: Makefile
	cd googletest && mkdir -p build && cd build && cmake -E env CXXFLAGS=--std=c++11 cmake ..
	$(MAKE) -C googletest/build
