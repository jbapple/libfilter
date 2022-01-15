MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: default world clean

default: bench.exe fpps.exe hibp.exe

world: default

clean:
	rm -f bench.exe bench.o bench.d bench.d.new
	rm -f fpps.exe fpps.o fpps.d fpps.d.new
	rm -f hibp.exe hibp.o hibp.d hibp.d.new

export CXXFLAGS += -O3 -ggdb3 -DNDEBUG

include $(DEFAULT_RECIPE)

include bench.d
include fpps.d
include hibp.d

bench.exe: $(PROJECT_ROOT)/c/lib/libfilter.a
fpps.exe: $(PROJECT_ROOT)/c/lib/libfilter.a
hibp.exe: $(PROJECT_ROOT)/c/lib/libfilter.a
