MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules
CXXFLAGS += --std=c++14

.PHONY: default world clean

default: cpp-example.exe

world: default

include $(DEFAULT_RECIPE)

include cpp-example.d

clean:
	rm -f cpp-example.exe cpp-example.o cpp-example.d cpp-example.d.new
