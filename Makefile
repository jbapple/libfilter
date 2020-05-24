.PHONY: world lib examples clean install libpcg_random benchmarks test googletest

lib: libpcg_random
	$(MAKE) -C lib

world: lib examples Makefile benchmarks test

examples: lib libpcg_random
	$(MAKE) -C examples

libpcg_random:
	$(MAKE) -C pcg-c

benchmarks: lib libpcg_random
	$(MAKE) -C benchmarks

test: lib googletest
	$(MAKE) -C test

googletest:
	cd googletest && mkdir -p build && cd build && cmake ..
	$(MAKE) -C googletest/build

clean:
	$(MAKE) -C lib clean
	$(MAKE) -C examples clean
	$(MAKE) -C pcg-c clean
	$(MAKE) -C benchmarks clean
	$(MAKE) -C test clean
	rm -rf googletest/build

install: lib libpcg_random include/memory.h include/simd-block.h include/simd-block.hpp include/util.h
	$(MAKE) -C pcg-c install
	install -d /usr/local/include/libfilter
	install lib/libfilter.a /usr/local/lib
	install -m 0644 include/memory.h include/simd-block.h include/simd-block.hpp include/util.h /usr/local/include/libfilter

