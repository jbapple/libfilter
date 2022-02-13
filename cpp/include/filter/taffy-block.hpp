#include <algorithm>
#include <cmath>
#include <numeric>
#include <cstdint>

extern "C" {
#include "filter/taffy-block.h"
}

namespace filter {

struct TaffyBlockFilter {
  libfilter_taffy_block data;
  ~TaffyBlockFilter() { libfilter_taffy_block_destruct(&data); }

  static TaffyBlockFilter CreateWithNdvFpp(uint64_t ndv, double fpp) {
    TaffyBlockFilter result;
    libfilter_taffy_block_init(ndv, fpp, &result.data);
    return result;
  }

  uint64_t SizeInBytes() const { return libfilter_taffy_block_size_in_bytes(&data); }

  bool InsertHash(uint64_t h) { return libfilter_taffy_block_add_hash(&data, h); }

  bool FindHash(uint64_t h) const { return libfilter_taffy_block_find_hash(&data, h); }

  static const char* Name() { return "TaffyBlock"; }
};

}  // namespace filter
