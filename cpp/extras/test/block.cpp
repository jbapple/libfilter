#include "filter/block.hpp"
#include "filter/elastic.hpp"
#include "filter/block-elastic.hpp"
#include "filter/minimal-plastic.hpp"

#include <cstdint>  // for uint64_t
#include <unordered_set>
#include <vector>  // for allocator, vector

#include "gtest/gtest.h"
#include "util.hpp"  // for Rand

using namespace filter;
using namespace std;

template <typename F>
class BlockTest : public ::testing::Test {};

template <typename F>
class GeneralTest : public ::testing::Test {};

template <typename F>
class BytesTest : public ::testing::Test {};

template <typename F>
class NdvFppTest : public ::testing::Test {};

using BlockTypes = ::testing::Types<BlockFilter, ScalarBlockFilter>;
using CreateWithBytes = ::testing::Types<MinimalPlasticFilter, BlockFilter, ScalarBlockFilter>;
using CreateWithNdvFpp = ::testing::Types<BlockElasticFilter>;

TYPED_TEST_SUITE(BlockTest, BlockTypes);
TYPED_TEST_SUITE(BytesTest, CreateWithBytes);
TYPED_TEST_SUITE(NdvFppTest, CreateWithNdvFpp);

// TODO: test hidden methods in libfilter.so

// TODO: test more methods, including copy

template <typename T>
void InsertPersistsHelp(T& x, vector<uint64_t>& hashes) {
  Rand r;
  // 200:
  r.xState = 0xc07db21402b59465;
  r.yState = 0x7489fa569760d5a5;
  r.initXState = r.xState;
  r.initYState = r.yState;
  for (unsigned i = 0; i < hashes.size(); ++i) {
    hashes[i] = r();
  }
  for (unsigned i = 0; i < hashes.size(); ++i) {
    x.InsertHash(hashes[i]);
    for (unsigned j = 0; j <= i; ++j) {
      if (not x.FindHash(hashes[j])) {
        throw 2;
      }
      assert(x.FindHash(hashes[j]));
      ASSERT_TRUE(x.FindHash(hashes[j]))
          << dec << j << " of " << i << " of " << hashes.size() << " with hash 0x" << hex
          << hashes[j];
    }
  }
}

// Test that once something is inserted, it's always present
TYPED_TEST(BytesTest, InsertPersistsWithBytes) {
  for (int i = 0; i < 32000; ++i) {
    auto ndv = 200;
    auto x = TypeParam::CreateWithBytes(ndv);
    vector<uint64_t> hashes(ndv);
    InsertPersistsHelp(x, hashes);
  }
}


// Test that once something is inserted, it's always present
TYPED_TEST(NdvFppTest, InsertPersistsWithNdvFpp) {
  auto ndv = 16000;
  auto x = TypeParam::CreateWithNdvFpp(ndv, 0.01);
  vector<uint64_t> hashes(ndv);
  InsertPersistsHelp(x, hashes);
}


template<typename T>
void StartEmptyHelp(const T& x, uint64_t ndv) {
  Rand r;
  for (uint64_t j = 0; j < ndv; ++j) {
    auto v = r();
    EXPECT_FALSE(x.FindHash(v)) << v;
  }
}


// Test that filters start with a 0.0 fpp.
TYPED_TEST(BytesTest, StartEmpty) {
  auto ndv = 16000000;
  auto x = TypeParam::CreateWithBytes(ndv);
  return StartEmptyHelp(x, ndv);
}

// Test that filters start with a 0.0 fpp.
TYPED_TEST(NdvFppTest, StartEmpty) {
  auto ndv = 16000000;
  auto fpp = 0.01;
  auto x = TypeParam::CreateWithNdvFpp(ndv, fpp);
  return StartEmptyHelp(x, ndv);
}

// Test that the hash value of a filter changes when something is added
TYPED_TEST(BlockTest, HashChanges) {
  auto ndv = 40000;
  auto x = TypeParam::CreateWithBytes(ndv);
  vector<uint64_t> entropy(libfilter_hash_tabulate_entropy_bytes / sizeof(uint64_t));
  vector<uint64_t> hashes(ndv);
  Rand r;
  for (unsigned i = 0; i < entropy.size(); ++i) {
    entropy[i] = r();
  }
  for (int i = 0; i < ndv; ++i) {
    hashes[i] = r();
  }
  unordered_set<uint64_t> old_hashes;
  for (auto h : hashes) {
    if (x.FindHash(h)) continue;
    x.InsertHash(h);
    auto after = x.SaltedHash(entropy.data());
    EXPECT_EQ(old_hashes.find(after), old_hashes.end());
    old_hashes.insert(after);
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

// Test eqaulity operator
TYPED_TEST(BlockTest, EqualStayEqual) {
  auto ndv = 160000;
  auto x = TypeParam::CreateWithBytes(ndv);
  auto y = TypeParam::CreateWithBytes(ndv);
  vector<uint64_t> hashes(ndv);
  Rand r;
  for (int i = 0; i < ndv; ++i) {
    hashes[i] = r();
  }
  for (int i = 0; i < ndv; ++i) {
    x.InsertHash(hashes[i]);
    y.InsertHash(hashes[i]);
    auto z = x;
    EXPECT_TRUE(x == y);
    EXPECT_TRUE(y == z);
    EXPECT_TRUE(z == x);
  }
}
