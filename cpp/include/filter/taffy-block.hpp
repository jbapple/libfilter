#include <cstdint>
#include <new>
#include <utility>

extern "C" {
#include "filter/taffy-block.h"
}

namespace filter {

struct TaffyBlockFilter {
  libfilter_taffy_block data;
  TaffyBlockFilter(const TaffyBlockFilter& that) {
    if (0 != libfilter_taffy_block_clone(&that.data, &data)) throw std::bad_alloc();
  }
  TaffyBlockFilter& operator=(const TaffyBlockFilter& that) {
    this->~TaffyBlockFilter();
    new (this) TaffyBlockFilter(that);
    return *this;
  }
  TaffyBlockFilter(TaffyBlockFilter&& that)
      : data(that.data) {
    for (int i = 0; i < 48; ++i) libfilter_block_zero_out(&that.data.levels[i]);
  }
  TaffyBlockFilter& operator=(TaffyBlockFilter&& that) {
    this->~TaffyBlockFilter();
    new (this) TaffyBlockFilter(std::move(that));
    return *this;
  }

  ~TaffyBlockFilter() { libfilter_taffy_block_destruct(&data); }

  static TaffyBlockFilter CreateWithNdvFpp(uint64_t ndv, double fpp) {
    TaffyBlockFilter result(ndv, fpp);
    return result;
  }

 private:
  TaffyBlockFilter(uint64_t ndv, double fpp) {
    libfilter_taffy_block_init(ndv, fpp, &data);
  }

  public:
  uint64_t SizeInBytes() const { return libfilter_taffy_block_size_in_bytes(&data); }

  bool InsertHash(uint64_t h) { return libfilter_taffy_block_add_hash(&data, h); }

  bool FindHash(uint64_t h) const { return libfilter_taffy_block_find_hash(&data, h); }

  static const char* Name() { return "TaffyBlock"; }
};

}  // namespace filter
