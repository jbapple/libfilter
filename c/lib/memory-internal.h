// Tools for allocating aligned memory, as needed for some SIMD loads and stores. Also
// alocates huge pages where possible.

#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "filter/memory.h"

typedef struct {
  libfilter_region region;
  // block_bytes is the amount of memory that is addressable via region.block. It has two
  // potential uses. When the memory was allocated with mmap, block_bytes is needed to
  // pass to munmap. As such, it must be possible to determine if mmap was used
  // by block_bytes (and feature-test macros).
  //
  // Additionally, whether mmap or aligned_alloc or malloc was used to allocate the
  // memory, block_bytes is a way of returning to callers of libfilter_alloc_at_most how
  // much memory they got. They passed in max_bytes, but it may not have been possible to
  // give back that much memory, since malloc may have returned unaligned memory.
  uint64_t block_bytes;
  // Indicates whether the block was zero-filled when allocated. Some allocation methods
  // guarantee this; others do not. MAP_ANONYMOUS on Linux guarantees zero-filling.
  bool zero_filled;
} libfilter_region_alloc_result;

// TODO: try split memory, with some in huge pages and the rest not.

libfilter_region_alloc_result __attribute__((visibility("hidden")))
libfilter_alloc_at_most(uint64_t max_bytes, uint64_t alignment);

int __attribute__((visibility("hidden")))
libfilter_do_free(libfilter_region r, uint64_t bytes, uint64_t alignment);

void __attribute__((visibility("hidden")))
libfilter_clear_region(libfilter_region* here);

uint64_t __attribute__((visibility("hidden")))
libfilter_new_alloc_request(uint64_t exact_bytes, uint64_t alignment);
