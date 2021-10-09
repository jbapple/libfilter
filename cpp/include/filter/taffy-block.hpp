#include <algorithm>
#include <cmath>
#include <numeric>
#include <cstdint>

#include "block.hpp"

namespace filter {

struct TaffyBlockFilter {
  BlockFilter* levels[64] = {};
  double predicted_fpps[64] = {};
  uint64_t sizes[64];
  int cursor = 0;
  uint64_t last_ndv = 1;
  int64_t ttl = 0;
  double fpp;

  ~TaffyBlockFilter() {
    for (int i = 0; i < cursor; ++i) {
      delete levels[i];
      levels[i] = nullptr;
    }
  }

 protected:
  TaffyBlockFilter(uint64_t ndv, double fpp) : last_ndv(ndv), ttl(ndv), fpp(fpp) {
    const double sum =  6.0 / std::pow(3.1415, 2);
    fpp = fpp / 0.4;
    ndv = std::max(ndv, BlockFilter::MaxCapacity(1, fpp * sum));
    last_ndv = ndv;
    ttl = ndv;
    levels[0] = new BlockFilter(BlockFilter::CreateWithNdvFpp(ndv, fpp * sum));
    ++cursor;
    for (uint64_t x = 0; x < 32; ++x) {
      sizes[x] =
          BlockFilter::MinSpaceNeeded(ndv << x, fpp / std::pow(x + 1, 2) * sum);
    }
  }

 public:
  static TaffyBlockFilter CreateWithNdvFpp(uint64_t ndv, double fpp) {
    return TaffyBlockFilter(ndv, fpp);
  }

  static const char* Name() { return "TaffyBlock"; }

  uint64_t SizeInBytes() const {
    uint64_t result = 0;
    for (int i = 0; i < cursor; ++i) {
      result += levels[i]->SizeInBytes();
    }
    return result;
  }

  void Upsize() {
    last_ndv *= 2;
    levels[cursor] = new BlockFilter(BlockFilter::CreateWithBytes(sizes[cursor]));
    ++cursor;
    ttl = last_ndv;
  }

  bool InsertHash(uint64_t h) {
    if (ttl <= 0) Upsize();
    levels[cursor - 1]->InsertHash(h);
    --ttl;
    return true;
  }

  bool FindHash(uint64_t h) const {
    for (int i = 0; i < cursor; ++i) {
      if (levels[i]->FindHash(h)) return true;
    }
    return false;
  }
};

}  // namespace filter
