#include "simd-block.hpp"

#include "gtest/gtest.h"

using namespace filter;


template <typename F>
class BlockTest : public ::testing::Test {};

using BlockTypes =
    ::testing::Types<BlockFilter<4, 32>, BlockFilter<4, 64>, BlockFilter<8, 32>,
                     BlockFilter<8, 64>, BlockFilter<16, 32>,

#define SHINGLE_ALL(X)                                            \
  ShingleBlockFilter<4, 32, X>, ShingleBlockFilter<4, 64, X>,     \
      ShingleBlockFilter<8, 32, X>, ShingleBlockFilter<8, 64, X>, \
      ShingleBlockFilter<16, 32, X>

                     SHINGLE_ALL(1), SHINGLE_ALL(2), SHINGLE_ALL(4), SHINGLE_ALL(8),
                     SHINGLE_ALL(16),

                     ShingleBlockFilter<4, 64, 32>, ShingleBlockFilter<8, 32, 32>,
                     ShingleBlockFilter<8, 64, 32>, ShingleBlockFilter<16, 32, 32>,

                     ShingleBlockFilter<8, 64, 64>, ShingleBlockFilter<16, 32, 64>>;
#undef SHINGLE_ALL

TYPED_TEST_SUITE(BlockTest, BlockTypes);

TYPED_TEST(BlockTest, InsertPersists) {
  auto ndv = 8000;
  auto x = TypeParam::CreateWithBytes(ndv);
  EXPECT_FALSE(x.FindHash(0));
  for (int i = 0; i < ndv; ++i) {
    x.InsertHash(i);
    for (int j = 0; j <= i; ++j) {
      EXPECT_TRUE(x.FindHash(j));
    }
  }
}

TYPED_TEST(BlockTest, StartEmpty) {
  auto ndv = 8000;
  auto x = TypeParam::CreateWithBytes(ndv);
  for (int j = 0; j < ndv; ++j) {
    EXPECT_FALSE(x.FindHash(j)) << j;
  }
}
