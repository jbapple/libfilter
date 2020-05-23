MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: default world clean

default: bench.exe fpps.exe

world: default

clean:
	rm -f bench.exe bench.o bench.d bench.d.new
	rm -f fpps.exe fpps.o fpps.d fpps.d.new

# export CXXFLAGS += -O0 -ggdb3

include $(DEFAULT_RECIPE)

include bench.d
include fpps.d

bench.exe: $(PROJECT_ROOT)/c/lib/libfilter.a
fpps.exe: $(PROJECT_ROOT)/c/lib/libfilter.a
