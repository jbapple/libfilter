#include "block.hpp"

namespace filter {

struct BlockElasticFilter {
  BlockFilter* levels[64] = {};
  uint64_t sizes[64];
  int cursor = 0;
  uint64_t last_ndv = 1;
  int64_t ttl = 0;
  double fpp;

  ~BlockElasticFilter() {
    for (int i = 0; i < cursor; ++i) {
      delete levels[i];
      levels[i] = nullptr;
    }
  }

 protected:
  BlockElasticFilter(uint64_t ndv, double fpp) : last_ndv(ndv), ttl(ndv), fpp(fpp) {
    levels[0] = new BlockFilter(BlockFilter::CreateWithNdvFpp(ndv, fpp / 1.65));
    ++cursor;
    for (uint64_t x = 0; x < 32; ++x) {
      sizes[x] = BlockFilter::MinSpaceNeeded(
          ndv << x, fpp / pow(cursor + 1, 2) * 6 / pow(3.1415, 2));
      //std::cout << sizes[x] << std::endl;
    }
  }

 public:
  static BlockElasticFilter CreateWithNdvFpp(uint64_t ndv, double fpp) {
    return BlockElasticFilter(ndv, fpp);
  }

  static const char* Name() { return "BlockElastic"; }

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
    //        last_ndv, fpp / pow(cursor + 1, 2) * 6 / pow(3.1415, 2)));
    // std::cout << cursor << " " << last_ndv << " " << levels[cursor]->SizeInBytes()
    //        << std::endl;
    ++cursor;
    ttl = last_ndv;
  }

  void InsertHash(uint64_t h) {
    if (ttl <= 0) Upsize();
    levels[cursor - 1]->InsertHash(h);
    --ttl;
  }

  bool FindHash(uint64_t h) const {
    for (int i = 0; i < cursor; ++i) {
      if (levels[i]->FindHash(h)) return true;
    }
    return false;
  }
};

}  // namespace filter
