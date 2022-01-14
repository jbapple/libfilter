#include "filter/taffy-block.h"

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

void libfitler_taffy_block_upsize(libfilter_taffy_block* here) {
  here->last_ndv *= 2;
  here->levels[here->cursor] = (libfilter_block*)malloc(sizeof(libfilter_block));
  libfilter_block_init(here->sizes[here->cursor], here->levels[here->cursor]);
  ++here->cursor;
  here->ttl = here->last_ndv;
}
