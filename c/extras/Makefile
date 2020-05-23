.PHONY: all examples clean

all: examples

export LIB_DEPS := $(C_ROOT)/lib/libfilter.a
export LINKS := $(LIB_DEPS) -lm

clean: Makefile
	$(MAKE) -C examples clean

examples: Makefile
	$(MAKE) -C examples
