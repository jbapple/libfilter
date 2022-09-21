.PHONY: default world clean googletest

default: block.exe

world: default

clean:
	rm -f block.exe block.o block.d block.d.new
	rm -rf googletest/build

LINKS += googletest/build/lib/libgtest_main.a googletest/build/lib/libgtest.a -lpthread -L${JAVA_HOME}/jre/lib/amd64/server -L${JAVA_HOME}/lib/server
INCLUDES += -I googletest/googletest/include
export CXXFLAGS := -std=c++17 $(INCLUDES) $(WARN) $(RELEASE) -ggdb3 # -UNDEBUG  -O0 -march=x86-64 -fsanitize=undefined #-fsanitize=address

COMPILER_VERSION := $(shell $(CC) --version)
ifneq '' '$(findstring apple-darwin,$(COMPILER_VERSION))'
  LINKS += -ljli
else
  LINKS += -ljvm
endif


include $(DEFAULT_RECIPE)

include block.d

googletest/build/lib/libgtest_main.a googletest/build/lib/libgtest.a: googletest

block.exe: googletest/build/lib/libgtest_main.a googletest/build/lib/libgtest.a

googletest: Makefile
	cd googletest && mkdir -p build && cd build && cmake -E env CXXFLAGS=--std=c++11 cmake ..
	$(MAKE) -C googletest/build
