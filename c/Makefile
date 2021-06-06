.PHONY: default clean install uninstall lib extras

default: lib

export DEFAULT_RECIPE := $(CURDIR)/common.mk
export INCLUDES := -I $(CURDIR)/include
export C_ROOT := $(CURDIR)

world: default extras

clean: Makefile
	$(MAKE) -C lib clean
	$(MAKE) -C extras clean

lib: Makefile
	$(MAKE) -C lib

extras: lib
	$(MAKE) -C extras

install: lib include/filter/memory.h include/filter/block.h include/filter/hash.h
	install -d /usr/local/include/filter
	install lib/libfilter.a /usr/local/lib
	install lib/libfilter.so /usr/local/lib
	install -m 0644 include/filter/memory.h include/filter/block.h include/filter/hash.h /usr/local/include/filter

uninstall:
	rm -f /usr/local/include/filter/memory.h
	rm -f /usr/local/include/filter/block.h
	rm -f /usr/local/lib/libfilter.a
	rm -f /usr/local/lib/libfilter.so
