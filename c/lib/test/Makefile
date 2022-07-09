MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: all clean

all: peel.exe

WARN=-W -Wall -Wextra
RELEASE=-march=native -mtune=native -O3 -ggdb3
CFLAGS=-std=c99 $(INCLUDES) $(WARN) $(RELEASE) -I..
LINKS=-lm

COMPILER_VERSION := $(shell $(CC) --version)
ifneq '' '$(findstring clang,$(COMPILER_VERSION))'
ifeq '' '$(findstring apple-darwin,$(COMPILER_VERSION))'
  CFLAGS += -fsanitize=memory
endif
endif

include $(DEFAULT_RECIPE)

include peel.d

peel.exe: $(LIB_DEPS)

clean:
	rm -f peel.exe peel.d peel.o peel.d.new
