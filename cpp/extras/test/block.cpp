#include <cstdint> // for uint64_t
#include <vector>  // for allocator, vector

#include "gtest/gtest.h"

#include "filter/block.hpp"
#include "util.hpp" // for Rand

using namespace filter;
using namespace std;

template <typename F>
class BlockTest : public ::testing::Test {};

using BlockTypes = ::testing::Types<BlockFilter, ScalarBlockFilter>;

TYPED_TEST_SUITE(BlockTest, BlockTypes);

// TODO: test hidden methods in libfilter.so

// TODO: test more methods, including copy

// Test that once something is inserted, it's always present
TYPED_TEST(BlockTest, InsertPersists) {
  auto ndv = 16000;
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
  auto ndv = 16000000;
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
    auto ndv = 1600000;
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
