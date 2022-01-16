.PHONY: all examples test clean

all: examples test

export LIB_DEPS := $(C_ROOT)/lib/libfilter.a
export LINKS := $(LIB_DEPS) -lm

clean: Makefile
	$(MAKE) -C examples clean
	$(MAKE) -C test clean

examples: Makefile
	$(MAKE) -C examples

test: Makefile
	$(MAKE) -C test
