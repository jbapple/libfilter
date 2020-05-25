#include "simd-block.hpp"
using namespace filter;

#include <vector>
using namespace std;

#include "gtest/gtest.h"

#include "util.hpp"

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

// Test that once something is inserted, it's always present
TYPED_TEST(BlockTest, InsertPersists) {
  auto ndv = 8000;
  auto x = TypeParam::CreateWithBytes(ndv);
  vector<uint64_t> hashes(ndv);
  Rand r;
  for (int i = 0; i < ndv; ++i) {
    hashes[i] = r();
  }
  for (int i = 0; i < ndv; ++i) {
    x.InsertHash(hashes[i]);
    for (int j = 0; j <= i; ++j) {
      EXPECT_TRUE(x.FindHash(hashes[j]));
    }
  }
}

// Test that filters start with a 0.0 fpp.
TYPED_TEST(BlockTest, StartEmpty) {
  auto ndv = 8000000;
  auto x = TypeParam::CreateWithBytes(ndv);
  Rand r;
  for (int j = 0; j < ndv; ++j) {
    auto v = r();
    EXPECT_FALSE(x.FindHash(v)) << v;
  }
}

// Test that Scalar and Simd variants are identical
TYPED_TEST(BlockTest, Buddy) {
  if (TypeParam::is_simd) {
    auto ndv = 800000;
    auto x = TypeParam::CreateWithBytes(ndv);
    auto y = TypeParam::Scalar::CreateWithBytes(ndv);
    Rand r;
    for (int i = 0; i < ndv; ++i) {
      auto v = r();
      x.InsertHash(v);
      y.InsertHash(v);
    }
    for (int i = 0; i < ndv; ++i) {
      auto v = r();
      EXPECT_EQ(x.FindHash(v), y.FindHash(v));
    }
  }
}
