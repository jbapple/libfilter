MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: all clean

all: paths.exe

WARN=-W -Wall -Wextra
RELEASE=-march=native -mtune=native -O3
CFLAGS=-std=c99 $(INCLUDES) $(WARN) $(RELEASE)

include $(DEFAULT_RECIPE)

include paths.d

paths.exe: $(LIB_DEPS)

clean:
	rm -f paths.exe paths.d paths.o paths.d.new
