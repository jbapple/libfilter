MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: default world clean

default: bench.exe fpps.exe hash.exe mpcf-cycle.exe

world: default

clean:
	rm -f bench.exe bench.o bench.d bench.d.new
	rm -f fpps.exe fpps.o fpps.d fpps.d.new
	rm -f hash.exe hash.o hash.d hash.d.new
	rm -f mpcf-cycle.exe mpcf-cycle.o mpcf-cycle.d mpcf-cycle.d.new

export CXXFLAGS += -O3 -ggdb3 -DNDEBUG

include $(DEFAULT_RECIPE)

include bench.d
include fpps.d
include hash.d
include mpcf-cycle.d

bench.exe: $(PROJECT_ROOT)/c/lib/libfilter.a
fpps.exe: $(PROJECT_ROOT)/c/lib/libfilter.a
hash.exe: $(PROJECT_ROOT)/c/lib/libfilter.a
