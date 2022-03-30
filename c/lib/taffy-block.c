#include "filter/taffy-block.h"

void libfilter_taffy_block_destruct(libfilter_taffy_block* here) {
  for (int i = 0; i < here->cursor; ++i) {
    libfilter_block_destruct(&here->levels[i]);
  }
}

int libfilter_taffy_block_init(uint64_t ndv, double fpp, libfilter_taffy_block * here) {
  here->last_ndv = ndv;
  here->ttl = ndv;
  here->cursor = 0;
  const double sum = 6.0 / pow(3.1415, 2);
  uint64_t ndv2 = libfilter_block_capacity(1, fpp * sum);
  ndv = (ndv > ndv2) ? ndv : ndv2;
  here->last_ndv = ndv;
  here->ttl = ndv;
  const int result = libfilter_block_init(libfilter_block_bytes_needed(ndv, fpp * sum),
                                          &here->levels[0]);
  ++here->cursor;
  for (uint64_t x = 0; x < 48; ++x) {
    here->sizes[x] = libfilter_block_bytes_needed(ndv << x, fpp / pow(x + 1, 2) * sum);
  }
  return result;
}

void libfilter_taffy_block_upsize(libfilter_taffy_block* here) {
  here->last_ndv *= 2;
  libfilter_block_init(here->sizes[here->cursor], &here->levels[here->cursor]);
  ++here->cursor;
  here->ttl = here->last_ndv;
}

int libfilter_taffy_block_clone(const libfilter_taffy_block* from,
                                libfilter_taffy_block* to) {
  for(int i = 0; i < 48; ++i) {
    int notok = libfilter_block_clone(&from->levels[i], &to->levels[i]);
    if (notok) return notok;
    to->sizes[i] = from->sizes[i];
  }
  to->cursor = from->cursor;
  to->last_ndv = from->last_ndv;
  to->ttl = from->ttl;
  return 0;
}
