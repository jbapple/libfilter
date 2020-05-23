// Tools for allocating aligned memory, as needed for some SIMD loads and stores. Also
// alocates huge pages where possible.

#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: macro push and pop
// TODO: macro push and pop when in VSCode?

// A region may have a different locations where a block starts and where free() should be
// called. This only occurs in the UNALIGNED case.
typedef struct {
  void* block;
  void* to_free;
} libfilter_region;

typedef struct {
  libfilter_region region;
  // block_bytes is the amount of memory that is addressable via region.block
  uint64_t block_bytes;
  // Indicates whether the block was zero-filled when allocated. Some allocation methods
  // guarantee this; others do not. MAP_ANONYMOUS on Linux guarantees zero-filling.
  bool zero_filled;
} libfilter_region_alloc_result;

// TODO: try _mm_malloc, __mingw_aligned_malloc, _aligned_malloc

// TODO: try split memory, with some in huge pages and the rest not.

libfilter_region_alloc_result __attribute__((visibility("hidden")))
libfilter_do_alloc(uint64_t max_bytes, uint64_t alignment);

bool __attribute__((visibility("hidden")))
libfilter_do_free(libfilter_region r, uint64_t bytes, uint64_t alignment);

void __attribute__((visibility("hidden")))
libfilter_clear_region(libfilter_region* here);
