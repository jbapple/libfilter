#include "simd-block.hpp"

#include "gtest/gtest.h"

using namespace filter;

TEST(BlockTest, InsertPersists) {
  auto x = BlockFilter<8, 32>::CreateWithBytes(4000);
  EXPECT_FALSE(x.FindHash(0));
  for (int i = 0; i < 1000; ++i) {
    x.InsertHash(i);
    for (int j = 0; j <= i; ++j) {
      EXPECT_TRUE(x.FindHash(j));
    }
  }
}
