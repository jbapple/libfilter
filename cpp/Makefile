#MAKEFLAGS += --warn-undefined-variables
#MAKEFLAGS += --no-builtin-rules

.PHONY: default world clean install uninstall

export CPP_ROOT := $(CURDIR)
export PROJECT_ROOT ?= $(CPP_ROOT)/..
export INCLUDES := -I $(PROJECT_ROOT)/c/include -I $(CPP_ROOT)/include
export LINKS := $(PROJECT_ROOT)/c/lib/libfilter.a -lm
export DEFAULT_RECIPE := $(CURDIR)/common.mk

default:

world:
	$(MAKE) -C extras world

clean:
	$(MAKE) -C extras clean

install:
	install -m 0644 include/filter/block.hpp /usr/local/include/filter

uninstall:
	rm -f /usr/local/include/filter/block.hpp
