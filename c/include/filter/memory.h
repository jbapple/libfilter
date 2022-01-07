// Tools for allocating aligned memory, as needed for some SIMD loads and stores. Also
// alocates huge pages where possible.

#pragma once

#include <stdint.h>

// A region may have a different locations where a block starts and where free() should be
// called. This only occurs in the UNALIGNED case.
typedef struct {
  // TODO: Is aligned at 32-byte boundary, but this can't be specified with _Alignas
  // because that's only in C11 and not available in C++, so will need to use alignas when
  // in C++ mode.
  uint32_t* block;
  void* to_free;
} libfilter_region;
