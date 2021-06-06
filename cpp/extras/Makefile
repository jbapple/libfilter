.PHONY: default world clean examples benchmarks test googletest

default: examples benchmarks test googletest

world: default

clean: Makefile
	$(MAKE) -C examples clean
	$(MAKE) -C benchmarks clean
	$(MAKE) -C test clean
	rm -rf googletest/build

export EXTRAS_ROOT := $(CURDIR)
export INCLUDES := $(INCLUDES) -I $(EXTRAS_ROOT)/include -I $(EXTRAS_ROOT)/googletest/googletest/include
export WARN=-W -Wall -Wextra
export RELEASE=-march=native -mtune=native -O3 -DNDEBUG # -ggdb3
export CXXFLAGS := -std=c++14 $(INCLUDES) $(WARN) $(RELEASE)

examples: Makefile
	$(MAKE) -C examples

benchmarks: Makefile
	$(MAKE) -C benchmarks

test: googletest Makefile
	$(MAKE) -C test
