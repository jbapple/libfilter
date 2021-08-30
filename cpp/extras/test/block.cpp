#include <cstdint>  // for uint64_t
#include <unordered_set>
#include <vector>  // for allocator, vector

#include "gtest/gtest.h"

#include "util.hpp"  // for Rand

#include "filter/block.hpp"
#include "filter/minimal-taffy-cuckoo.hpp"
#include "filter/taffy-block.hpp"
#include "filter/taffy-cuckoo.hpp"

using namespace filter;
using namespace std;

template <typename F>
class BlockTest : public ::testing::Test {};

template <typename F>
class UnionTest : public ::testing::Test {};

template <typename F>
class BytesTest : public ::testing::Test {};

template <typename F>
class NdvFppTest : public ::testing::Test {};

using BlockTypes = ::testing::Types<BlockFilter, ScalarBlockFilter>;
using CreateWithBytes = ::testing::Types<MinimalTaffyCuckooFilter, BlockFilter, ScalarBlockFilter, TaffyCuckooFilter>;
using CreateWithNdvFpp = ::testing::Types<TaffyBlockFilter>;
using UnionTypes = ::testing::Types<TaffyCuckooFilter>;

TYPED_TEST_SUITE(BlockTest, BlockTypes);
TYPED_TEST_SUITE(BytesTest, CreateWithBytes);
TYPED_TEST_SUITE(NdvFppTest, CreateWithNdvFpp);
TYPED_TEST_SUITE(UnionTest, UnionTypes);
// TODO: test hidden methods in libfilter.so

// TODO: test more methods, including copy

TYPED_TEST(UnionTest, UnionDoes) {
  for (unsigned xndv = 13; xndv < 14; ++xndv) {
    for (unsigned yndv = 469; yndv < 470; yndv += xndv) {
      // cout << "ndv " << dec << xndv << " " << yndv << endl;
      Rand r;
      vector<uint64_t> xhashes, yhashes;
      auto x = TypeParam::CreateWithBytes(0);
      auto y = TypeParam::CreateWithBytes(0);
      for (unsigned i = 0; i < xndv; ++i) {
        xhashes.push_back(r());
        x.InsertHash(xhashes.back());
      }
      for (unsigned i = 0; i < yndv; ++i) {
        yhashes.push_back(r());
        y.InsertHash(yhashes.back());
      }
      auto z = Union(x, y);
      for (unsigned j = 0; j < xhashes.size(); ++j) {
        EXPECT_TRUE(z.FindHash(xhashes[j]))
            << xndv << " " << yndv << " " << j << " " << hex << "0x" << xhashes[j];
      }
      for (unsigned j = 0; j < yhashes.size(); ++j) {
        EXPECT_TRUE(z.FindHash(yhashes[j]))
            << xndv << " " << yndv << " " << j << " " << hex << "0x" << yhashes[j];
      }
    }
  }
  unsigned ndv = 80;
  auto x = TypeParam::CreateWithBytes(0);
  auto y = TypeParam::CreateWithBytes(0);
  vector<uint64_t> xhashes, yhashes;
  Rand r;
  for (unsigned i = 0; i < ndv; ++i) {
    xhashes.push_back(r());
    x.InsertHash(xhashes.back());
    yhashes.push_back(r());
    y.InsertHash(yhashes.back());
  }

  auto z = Union(x, y);
  auto p = x;
  auto q = y;
  for (unsigned j = 0; j < xhashes.size(); ++j) {
    ASSERT_TRUE(p.FindHash(xhashes[j])) << j << " " << hex << "0x" << xhashes[j];
    ASSERT_TRUE(q.FindHash(yhashes[j])) << j << " " << hex << "0x" << yhashes[j];
  }
  for (unsigned j = 0; j < xhashes.size(); ++j) {
    EXPECT_TRUE(z.FindHash(xhashes[j])) << j << " " << hex << "0x" << xhashes[j];
  }
  for (unsigned j = 0; j < yhashes.size(); ++j) {
    EXPECT_TRUE(z.FindHash(yhashes[j])) << j << " " << hex << "0x" << yhashes[j];
  }
}

template <typename T>
void InsertPersistsHelp(T& x, vector<uint64_t>& hashes) {
  Rand r;

  for (unsigned i = 0; i < hashes.size(); ++i) {
    hashes[i] = r();
  }
  for (unsigned i = 0; i < hashes.size(); ++i) {
    x.InsertHash(hashes[i]);
    for (unsigned j = 0; j <= i; ++j) {
      EXPECT_TRUE(x.FindHash(hashes[j]))
          << dec << j << " of " << i << " of " << hashes.size() << " with hash 0x" << hex
          << hashes[j];
      if (not x.FindHash(hashes[j])) {
        throw 2;
      }
    }
  }
}

// Test that once something is inserted, it's always present
TYPED_TEST(BytesTest, InsertPersistsWithBytes) {
  auto ndv = 16000;
  auto x = TypeParam::CreateWithBytes(ndv);
  vector<uint64_t> hashes(ndv);
  InsertPersistsHelp(x, hashes);
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
