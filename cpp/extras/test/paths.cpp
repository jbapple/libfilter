#include "filter/paths.hpp"

using namespace filter;
using namespace filter::detail;
using namespace filter::detail::minimal_plastic;

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
  unsigned many  = 0;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      for (bool is_short : {true, false}) {
        auto x = xbase << i;
        auto p = ToPath(x, f, cursor, low_level, is_short);
        if (p.tail == 0) continue;
        ++many;
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
  EXPECT_GE(many, 64 * 32u);
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


// TEST(Main, RePathIdentity) {
//   uint64_t entropy[4] = {1, 0, 1, 0};
//   Feistel identity(entropy);
//   uint64_t xbase = 0x1234'5678'9abc'def0;
//   auto low_level = 16;
//   for (int i = 0; i < 64; ++i) {
//     for (int cursor = 0; cursor < 32; ++cursor) {
//       for (bool is_short : {true, false}) {
//         auto x = xbase << i;
//         auto p = ToPath(x, identity, cursor, low_level, is_short);
//         if (p.tail == 0) continue;
//         Path r;
//         auto q = RePath(p, identity, identity, identity, identity, low_level, cursor,
//                         cursor, &r);
//         EXPECT_EQ(r.tail, 0u);
//         EXPECT_EQ(p.level, q.level);
//         EXPECT_EQ(p.bucket, q.bucket);
//         EXPECT_EQ(p.fingerprint, q.fingerprint);
//         EXPECT_EQ(p.long_fp, q.long_fp);
//         EXPECT_EQ(p.tail, q.tail);
//       }
//     }
//   }
// }


TEST(Main, RePathHalfIdentity) {
  uint64_t entropy[4] = {1, 0, 1, 0};
  Feistel identity(entropy);
  uint64_t entropy2[4] = {0x37156873ab534ce7, 0x5c669c3116114489, 0xfa52f24f2bc644d6,
                          0xcba217328d2f4950};
  Feistel f(entropy2);
  uint64_t xbase = 0x1234'5678'9abc'def0;
  auto low_level = 16;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      for (bool is_short : {false}) {
        auto x = xbase << i;
        auto p = ToPath(x, identity, cursor, low_level, is_short);
        if (p.tail == 0) continue;
        Path r;
        auto q =
            RePath(p, identity, identity, f, f, low_level, low_level, cursor, cursor, &r);
        EXPECT_EQ(r.tail, 0u);
        r = ToPath(x, f, cursor, low_level, is_short);
        EXPECT_NE(r.tail, 0u);
        EXPECT_EQ(q.level, r.level);
        EXPECT_EQ(q.bucket, r.bucket);
        EXPECT_EQ(q.fingerprint, r.fingerprint);
        EXPECT_EQ(q.long_fp, r.long_fp);
        EXPECT_EQ(q.tail, r.tail);
      }
    }
  }
}

TEST(Main, RePathShortIdentity) {
  uint64_t entropy[4] = {1, 0, 1, 0};
  Feistel identity(entropy);
  uint64_t entropy2[4] = {0x37156873ab534ce7, 0x5c669c3116114489, 0xfa52f24f2bc644d6,
                          0xcba217328d2f4950};
  Feistel f(entropy2);
  uint64_t xbase = 0x1234'5678'9abc'def0;
  auto low_level = 16;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      for (bool is_short : {true}) {
        auto x = xbase << i;
        auto p = ToPath(x, identity, cursor, low_level, is_short);
        auto q = ToPath(x, f, cursor, low_level, is_short);
        if (p.tail == 0 || q.tail == 0) {
          continue;
        }
        Path r;
        auto s =
            RePath(p, identity, identity, f, f, low_level, low_level, cursor, cursor, &r);
        EXPECT_EQ(r.tail, 0u);
        EXPECT_EQ(q.level, s.level);
        EXPECT_EQ(q.bucket, s.bucket);
        EXPECT_EQ(q.fingerprint, s.fingerprint);
        EXPECT_EQ(q.long_fp, s.long_fp);
        EXPECT_EQ(q.tail, s.tail);
      }
    }
  }
}

TEST(Main, RePathShortLongIdentity) {
  uint64_t entropy[4] = {1, 0, 1, 0};
  Feistel identity(entropy);
  uint64_t entropy2[4] = {0x37156873ab534ce7, 0x5c669c3116114489, 0xfa52f24f2bc644d6,
                          0xcba217328d2f4950};
  Feistel f(entropy2);
  uint64_t xbase = 0x1234'5678'9abc'def0;
  auto low_level = 16;
  unsigned count = 0;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      auto x = xbase << i;
      auto p = ToPath(x, identity, cursor, low_level, true);
      if (p.tail == 0) continue;
      auto q = ToPath(x, f, cursor, low_level, true);
      if (q.tail != 0) continue;
      count++;
      q = ToPath(x, f, cursor, low_level, false);
      Path r;
      auto s = RePath(p, identity, identity, f, f, low_level, low_level, cursor, cursor, &r);
      EXPECT_EQ(r.tail, 0u);
      EXPECT_EQ(q.level, s.level) << "here " << i << " " << cursor;
      EXPECT_EQ(q.bucket, s.bucket) << i << " " << cursor;
      EXPECT_EQ(q.fingerprint, s.fingerprint);
      EXPECT_EQ(q.long_fp, s.long_fp);
      EXPECT_TRUE(IsPrefixOf(s.tail, q.tail)) << q.tail << " " << s.tail;
    }
  }
  EXPECT_GE(count, 100u);
}


TEST(Main, RePathDouble) {
  uint64_t entropy[4] = {1, 0, 1, 0};
  Feistel identity(entropy);
  uint64_t entropy2[4] = {0x37156873ab534ce7, 0x5c669c3116114489, 0xfa52f24f2bc644d6,
                          0xcba217328d2f4950};
  Feistel f(entropy2);
  uint64_t xbase = 0x1234'5678'9abc'def0;
  auto low_level = 16;
  unsigned count = 0;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      auto x = xbase << i;
      auto p = ToPath(x, identity, cursor, low_level, true);
      if (p.tail == 0) continue;
      auto q = ToPath(x, f, cursor, low_level, true);
      if (q.tail != 0) continue;
      count++;
      q = ToPath(x, f, cursor, low_level, false);
      Path r;
      p.tail = 1u << kTailSize;
      auto s =
          RePath(p, identity, identity, f, f, low_level, low_level, cursor, cursor, &r);
      EXPECT_EQ(s.tail, 1u << kTailSize);
      EXPECT_EQ(r.tail, 1u << kTailSize);
      EXPECT_TRUE((q.level == s.level && q.bucket == s.bucket &&
                   q.fingerprint == s.fingerprint && q.long_fp == s.long_fp) ||
                  (q.level == r.level && q.bucket == r.bucket &&
                   q.fingerprint == r.fingerprint && q.long_fp == r.long_fp))
          << q.level << " " << s.level << " " << r.level;
    }
  }
  EXPECT_GE(count, 100u);
}
