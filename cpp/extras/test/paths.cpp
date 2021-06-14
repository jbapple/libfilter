#include "filter/paths.hpp"

using namespace filter;
using namespace filter::detail;

#include <cstdint>  // for uint64_t
#include <unordered_set>
#include <vector>  // for allocator, vector
#include <iostream>
#include <iomanip>

using namespace std;

#include "gtest/gtest.h"

TEST(Main, FromTo) {
  uint64_t entropy[4] = {1, 0, 1, 0};
  Feistel f(entropy);
  uint64_t xbase = 0x1234'5678'9abc'def0;
  auto low_level = 16;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      for (bool is_short : {true, false}) {
        auto x = xbase << i;
        auto p = ToPath(x, f, cursor, low_level, is_short);
        if (p.tail == 0) continue;
        uint64_t y = FromPathNoTail(p, f, low_level + (p.level < cursor),
                                    kHeadSize - (1 - p.long_fp));
        int shift = 64 - (kLogLevels + low_level + (p.level < cursor) + kHeadSize -
                          (1 - p.long_fp));
        EXPECT_EQ(x >> shift, y >> shift) << dec << i << endl
                                          << shift << endl
                                          << hex << (x >> shift) << endl
                                          << (y >> shift);
      }
    }
  }
}

TEST(Main, ToFromTo) {
  uint64_t entropy[4] = {1, 0, 1, 0};
  Feistel f(entropy);
  uint64_t xbase = 0x1234'5678'9abc'def0;
  auto low_level = 16;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      for (bool is_short : {true, false}) {
        auto x = xbase << i;
        auto p = ToPath(x, f, cursor, low_level, is_short);
        if (p.tail == 0) continue;
        uint64_t y = FromPathNoTail(p, f, low_level + (p.level < cursor),
                                    kHeadSize - (1 - p.long_fp));
        Path q = ToPath(y, f, cursor, low_level, is_short);
        EXPECT_EQ(p.level, q.level);
        EXPECT_EQ(p.bucket, q.bucket);
        EXPECT_EQ(p.fingerprint, q.fingerprint);
        EXPECT_EQ(p.long_fp, q.long_fp);
        // EXPECT_EQ(0u, q.tail);
      }
    }
  }
}
