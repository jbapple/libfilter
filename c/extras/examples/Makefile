MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: all clean

all: c-example.exe

WARN=-W -Wall -Wextra
RELEASE=-march=native -mtune=native -O3
CFLAGS=-std=c99 $(INCLUDES) $(WARN) $(RELEASE)

include $(DEFAULT_RECIPE)

include c-example.d

c-example.exe: $(LIB_DEPS)

clean:
	rm -f c-example.exe c-example.d c-example.o c-example.d.new
