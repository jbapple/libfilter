#include <algorithm>
#include <cmath>
#include <numeric>
#include <cstdint>

#include "block.hpp"

namespace filter {

extern "C" {

#include <stdbool.h>

#include "filter/block.h"

struct TaffyBlockFilterBase {
  libfilter_block* levels[64];
  uint64_t sizes[64];
  int cursor;
  uint64_t last_ndv;
  int64_t ttl;
};

void TaffyBlockFilterDestroy(TaffyBlockFilterBase* here) {
  for (int i = 0; i < here->cursor; ++i) {
    libfilter_block_destruct(here->levels[i]);
    here->levels[i] = NULL;
  }
}

TaffyBlockFilterBase TaffyBlockFilterCreateWithNdvFpp(uint64_t ndv, double fpp) {
  TaffyBlockFilterBase result;
  result.last_ndv = ndv;
  result.ttl = ndv;
  result.cursor = 0;
  const double sum = 6.0 / std::pow(3.1415, 2);
  ndv = std::max(ndv, libfilter_block_capacity(1, fpp * sum));
  result.last_ndv = ndv;
  result.ttl = ndv;
  result.levels[0] = (libfilter_block*)malloc(sizeof(libfilter_block));
  libfilter_block_init(libfilter_block_bytes_needed(ndv, fpp * sum), result.levels[0]);
  ++result.cursor;
  for (uint64_t x = 0; x < 32; ++x) {
    result.sizes[x] = libfilter_block_bytes_needed(ndv << x, fpp / std::pow(x + 1, 2) * sum);
  }
  return result;
}

uint64_t TaffyBlockFilterSizeInBytes(const TaffyBlockFilterBase* here) {
  uint64_t result = 0;
  for (int i = 0; i < here->cursor; ++i) {
    result += libfilter_block_size_in_bytes(here->levels[i]);
  }
  return result;
}

void TaffyBlockFilterUpsize(TaffyBlockFilterBase* here) {
  here->last_ndv *= 2;
  here->levels[here->cursor] = (libfilter_block*)malloc(sizeof(libfilter_block));
  libfilter_block_init(here->sizes[here->cursor], here->levels[here->cursor]);
  ++here->cursor;
  here->ttl = here->last_ndv;
}

bool TaffyBlockFilterInsertHash(TaffyBlockFilterBase* here, uint64_t h) {
  if (here->ttl <= 0) TaffyBlockFilterUpsize(here);
  libfilter_block_add_hash(h, here->levels[here->cursor - 1]);
  --here->ttl;
  return true;
}

bool TaffyBlockFilterFindHash(const TaffyBlockFilterBase* here, uint64_t h) {
  for (int i = 0; i < here->cursor; ++i) {
    if (libfilter_block_find_hash(h, here->levels[i])) return true;
  }
  return false;
}

}

struct TaffyBlockFilter {
  TaffyBlockFilterBase data;
  ~TaffyBlockFilter() { TaffyBlockFilterDestroy(&data); }

  static TaffyBlockFilter CreateWithNdvFpp(uint64_t ndv, double fpp) {
    return TaffyBlockFilter{TaffyBlockFilterCreateWithNdvFpp(ndv, fpp)};
  }

  uint64_t SizeInBytes() const { return TaffyBlockFilterSizeInBytes(&data); }

  void Upsize() { TaffyBlockFilterUpsize(&data);}

  bool InsertHash(uint64_t h) { return TaffyBlockFilterInsertHash(&data, h); }

  bool FindHash( uint64_t h) const { return TaffyBlockFilterFindHash(&data, h); }

  static const char* Name() { return "TaffyBlock"; }
};

}  // namespace filter
