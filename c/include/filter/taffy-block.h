#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "filter/block.h"
#include "filter/util.h"  // INLINE

typedef struct {
  libfilter_block* levels[48];
  uint64_t sizes[48];
  int cursor;
  uint64_t last_ndv;
  int64_t ttl;
} libfilter_taffy_block;

void libfilter_taffy_block_destroy(libfilter_taffy_block* here);

libfilter_taffy_block libfilter_taffy_block_create(uint64_t ndv, double fpp);

INLINE uint64_t libfilter_taffy_block_size_in_bytes(const libfilter_taffy_block* here) {
  uint64_t result = 0;
  for (int i = 0; i < here->cursor; ++i) {
    result += libfilter_block_size_in_bytes(here->levels[i]);
  }
  return result;
}

void libfitler_taffy_block_upsize(libfilter_taffy_block* here);

INLINE bool libfilter_taffy_block_insert_hash(libfilter_taffy_block* here, uint64_t h) {
  if (here->ttl <= 0) libfitler_taffy_block_upsize(here);
  libfilter_block_add_hash(h, here->levels[here->cursor - 1]);
  --here->ttl;
  return true;
}

INLINE bool libfilter_taffy_block_find_hash(const libfilter_taffy_block* here,
                                            uint64_t h) {
  for (int i = 0; i < here->cursor; ++i) {
    if (libfilter_block_find_hash(h, here->levels[i])) return true;
  }
  return false;
}
