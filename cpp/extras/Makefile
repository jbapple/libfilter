.PHONY: default world clean examples benchmarks test

default: examples benchmarks test

world: default

clean: Makefile
	$(MAKE) -C examples clean
	$(MAKE) -C benchmarks clean
	$(MAKE) -C test clean

export EXTRAS_ROOT := $(CURDIR)
export INCLUDES := $(INCLUDES) -I $(EXTRAS_ROOT)/include
export WARN=-W -Wall -Wextra
export RELEASE=-march=native -mtune=native -O3 # -ggdb3
export CXXFLAGS := -std=c++11 $(INCLUDES) $(WARN) $(RELEASE)

examples: Makefile
	$(MAKE) -C examples

benchmarks: Makefile
	$(MAKE) -C benchmarks

test: Makefile
	$(MAKE) -C test
