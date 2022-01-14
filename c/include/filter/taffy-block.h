#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include "filter/block.h"

typedef struct {
  libfilter_block* levels[64];
  uint64_t sizes[64];
  int cursor;
  uint64_t last_ndv;
  int64_t ttl;
} libfilter_taffy_block;

void libfilter_taffy_block_destroy(libfilter_taffy_block* here) {
  for (int i = 0; i < here->cursor; ++i) {
    libfilter_block_destruct(here->levels[i]);
    here->levels[i] = NULL;
  }
}

libfilter_taffy_block libfilter_taffy_block_create(uint64_t ndv, double fpp) {
  libfilter_taffy_block result;
  result.last_ndv = ndv;
  result.ttl = ndv;
  result.cursor = 0;
  const double sum = 6.0 / pow(3.1415, 2);
  uint64_t ndv2 = libfilter_block_capacity(1, fpp * sum);
  ndv = (ndv > ndv2) ? ndv : ndv2;
  result.last_ndv = ndv;
  result.ttl = ndv;
  result.levels[0] = (libfilter_block*)malloc(sizeof(libfilter_block));
  libfilter_block_init(libfilter_block_bytes_needed(ndv, fpp * sum), result.levels[0]);
  ++result.cursor;
  for (uint64_t x = 0; x < 64; ++x) {
    result.sizes[x] = libfilter_block_bytes_needed(ndv << x, fpp / pow(x + 1, 2) * sum);
  }
  return result;
}

uint64_t libfilter_taffy_block_size_in_bytes(const libfilter_taffy_block* here) {
  uint64_t result = 0;
  for (int i = 0; i < here->cursor; ++i) {
    result += libfilter_block_size_in_bytes(here->levels[i]);
  }
  return result;
}

void libfitler_taffy_block_upsize(libfilter_taffy_block* here) {
  here->last_ndv *= 2;
  here->levels[here->cursor] = (libfilter_block*)malloc(sizeof(libfilter_block));
  libfilter_block_init(here->sizes[here->cursor], here->levels[here->cursor]);
  ++here->cursor;
  here->ttl = here->last_ndv;
}

bool libfilter_taffy_block_insert_hash(libfilter_taffy_block* here, uint64_t h) {
  if (here->ttl <= 0) libfitler_taffy_block_upsize(here);
  libfilter_block_add_hash(h, here->levels[here->cursor - 1]);
  --here->ttl;
  return true;
}

bool libfilter_taffy_block_find_hash(const libfilter_taffy_block* here, uint64_t h) {
  for (int i = 0; i < here->cursor; ++i) {
    if (libfilter_block_find_hash(h, here->levels[i])) return true;
  }
  return false;
}
