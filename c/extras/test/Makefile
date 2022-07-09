MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: all clean

all: paths.exe static.exe

WARN=-W -Wall -Wextra
RELEASE=-march=native -mtune=native -O0 -ggdb3
CFLAGS=-std=c99 $(INCLUDES) $(WARN) $(RELEASE)

COMPILER_VERSION := $(shell $(CC) --version)
ifneq '' '$(findstring clang,$(COMPILER_VERSION))'
ifeq '' '$(findstring apple-darwin,$(COMPILER_VERSION))'
  CFLAGS += -fsanitize=address
endif
endif

include $(DEFAULT_RECIPE)

include paths.d
include static.d

paths.exe: $(LIB_DEPS)
static.exe: $(LIB_DEPS)

clean:
	rm -f paths.exe paths.d paths.o paths.d.new
	rm -f static.exe static.d static.o static.d.new
