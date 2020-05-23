// Tools for allocating aligned memory, as needed for some SIMD loads and stores. Also
// alocates huge pages where possible.

#pragma once

// A region may have a different locations where a block starts and where free() should be
// called. This only occurs in the UNALIGNED case.
typedef struct {
  void* block;
  void* to_free;
} libfilter_region;
