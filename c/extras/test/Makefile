MAKEFLAGS += --warn-undefined-variables
MAKEFLAGS += --no-builtin-rules

.PHONY: all clean

all: paths.exe peel.exe

WARN=-W -Wall -Wextra
RELEASE=-march=native -mtune=native -O3 -ggdb3 -DNDEBUG
CFLAGS=-std=c99 $(INCLUDES) $(WARN) $(RELEASE)

COMPILER_VERSION := $(shell $(CC) --version)
ifneq '' '$(findstring clang,$(COMPILER_VERSION))'
ifeq '' '$(findstring apple-darwin,$(COMPILER_VERSION))'
  CFLAGS += -fsanitize=memory
endif
endif


include $(DEFAULT_RECIPE)

include paths.d
include peel.d

paths.exe: $(LIB_DEPS)

clean:
	rm -f paths.exe paths.d paths.o paths.d.new
	rm -f peel.exe peel.d peel.o peel.d.new
