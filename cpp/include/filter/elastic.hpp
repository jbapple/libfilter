// Usable under the terms in the Apache License, Version 2.0.
//
// The elastic filter can, unlike Cuckoo filters and Bloom filters, increase in size to
// accommodate new items as the working set grows, without major degradation to the
// storage efficiency (size * false positive probability). Roughly speaking, it is a
// cuckoo hash table that uses "quotienting" for a succinct representation.
//
// See "How to Approximate A Set Without Knowing Its Size In Advance", by
// Rasmus Pagh, Gil Segev, and Udi Wieder

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <utility>

#include "immintrin.h"

#define INLINE __attribute__((always_inline)) inline

namespace filter {

namespace detail {

INLINE uint64_t Mask(int w, uint64_t x) { return x & ((1ul << w) - 1); }

// A permutation hash function based on 2-independent multiply-shift
struct Feistel {
  uint64_t keys[3][2];

  INLINE uint64_t Lo(int, int, int w, uint64_t x) const { return Mask(w, x); }
  INLINE uint64_t Hi(int s, int t, int w, uint64_t x) const {
    return Mask(w, x >> (s + t - w));
  }

  INLINE Feistel(const void* entropy) { memcpy(keys, entropy, 3 * 2 * sizeof(uint64_t)); }

  INLINE uint64_t ApplyOnce(int s, int t, int w, uint64_t x, const uint64_t k[2]) const {
    return Hi(s, t, s + t - w, Mask(w, x) * Mask(s + t, k[0]) + Mask(s + t, k[1]));
  }

  INLINE uint64_t Permute(int w, uint64_t x) const {
    auto s = w >> 1;
    auto t = w - s;

    auto l0 = Lo(s, t, s, x);
    auto r0 = Hi(s, t, t, x);

    auto l1 = r0;                                    // t
    auto r1 = l0 ^ ApplyOnce(s, t, t, r0, keys[0]);  // s

    auto l2 = r1;                                    // s
    auto r2 = l1 ^ ApplyOnce(s, t, s, r1, keys[1]);  // t

    auto result = (r2 << s) | l2;
    return result;
  }

  INLINE uint64_t ReversePermute(int w, uint64_t x) const {
    auto s = w / 2;
    auto t = w - s;

    auto l2 = Lo(s, t, s, x);
    auto r2 = Hi(s, t, t, x);

    auto r1 = l2;                                    // s
    auto l1 = r2 ^ ApplyOnce(s, t, s, r1, keys[1]);  // t

    auto r0 = l1;                                    // t
    auto l0 = r1 ^ ApplyOnce(s, t, t, r0, keys[0]);  // s

    auto result = (r0 << s) | l0;
    return result;
  }
};

}  // namespace detail
}  // namespace filter

namespace std {
INLINE void swap(filter::detail::Feistel& x, filter::detail::Feistel& y) {
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 2; ++j) {
      using std::swap;
      swap(x.keys[i][j], y.keys[i][j]);
    }
  }
}
}  // namespace std

namespace filter {
namespace detail {

struct PcgRandom {
  // *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
  // Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

  int bit_width;
  uint64_t state = 0x13d26df6f74044b3;
  uint64_t inc = 0x0d09b2d3025545a0;

  uint32_t current = 0;
  int remaining_bits = 0;

  INLINE uint32_t Get() {
    // Save some bits for next time
    if (remaining_bits >= bit_width) {
      auto result = current & ((1 << bit_width) - 1);
      current = current >> bit_width;
      remaining_bits = remaining_bits - bit_width;
      return result;
    }

    uint64_t oldstate = state;
    // Advance internal state
    state = oldstate * 6364136223846793005ULL + (inc | 1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    current = (xorshifted >> rot) | (xorshifted << ((-rot) & 31));

    remaining_bits = 32 - bit_width;
    auto result = current & ((1 << bit_width) - 1);
    current = current >> bit_width;
    return result;
  }

  INLINE uint32_t max() const { return ((1 << bit_width) - 1); }
  INLINE uint32_t min() const { return 0; }
  INLINE uint32_t operator()() { return Get(); }
};

// constexpr

// BMI LZCNT
INLINE bool IsPrefixOf(uint16_t x, uint16_t y) {
  assert(x != 0);
  assert(y != 0);
  auto a = x ^ y;
  // auto b = __builtin_ctz(a);
  auto c = __builtin_ctz(x);
  // return (b < c);
  auto h = __builtin_ctz(y);
#if defined(__LZCNT__)
  int i = _lzcnt_u32(a);
#else
  auto i = (a == 0) ? 32 : __builtin_clz(a);
#endif
  return (c >= h) && (i >= 31 - c);
}

// static_assert(IsPrefixOf(2, 1), "IsPrefixOf(2, 1)");
// static_assert(IsPrefixOf(2, 3), "IsPrefixOf(2, 3)");
// static_assert(IsPrefixOf(4, 1), "IsPrefixOf(4, 1)");

// static_assert(not IsPrefixOf(1, 3), "IsPrefixOf(1, 3)");
// static_assert(not IsPrefixOf(1, 2), "IsPrefixOf(1, 2)");

// static_assert(not IsPrefixOf(3, 1), "IsPrefixOf(3, 1)");
// static_assert(not IsPrefixOf(3, 2), "IsPrefixOf(3, 2)");
// static_assert(not IsPrefixOf(5, 2), "IsPrefixOf(5, 2)");
// static_assert(not IsPrefixOf(6, 2), "IsPrefixOf(6, 2)");
// static_assert(not IsPrefixOf(7, 2), "IsPrefixOf(7, 2)");

// static_assert(not IsPrefixOf(2, 5), "IsPrefixOf(2, 5)");
// static_assert(not IsPrefixOf(2, 6), "IsPrefixOf(2, 6)");
// static_assert(not IsPrefixOf(2, 7), "IsPrefixOf(2, 7)");

// static_assert(IsPrefixOf(16384, 1), "IsPrefixOf(16384, 1)");

// Return true (and the combined value) if x and y are the same except for their last
// digit.

/*
constexpr std::std::pair<bool, uint16_t> Combinable(uint16_t x, uint16_t y) {
  auto xtail = ((x ^ (x - 1)) + 1) / 2;
  auto ytail = ((y ^ (y - 1)) + 1) / 2;
  bool tail_eq = ((x | (xtail * 2)) == y) ||  // clang-format alignment
                 ((y | (ytail * 2)) == x);
  return {tail_eq, (x + y) / 2};
}

static_assert(Combinable(1, 3) == std::pair<bool, uint16_t>(true, 2), "Combinable(1, 3)");
static_assert(Combinable(5, 7) == std::pair<bool, uint16_t>(true, 6), "Combinable(5, 7)");
static_assert(Combinable(2, 6) == std::pair<bool, uint16_t>(true, 4), "Combinable(2, 6)");
static_assert(Combinable(1, 5).first == false, "Combinable(1, 5)");
static_assert(Combinable(1, 6).first == false, "Combinable(1, 6)");
*/

thread_local constexpr bool kDebug = false;

thread_local const constexpr int kHeadSize = 10;
thread_local const constexpr int kTailSize = 5;
static_assert(kHeadSize + kTailSize == 15, "kHeadSize + kTailSize == 15");

thread_local const constexpr int kLogBuckets = 2;
thread_local const constexpr int kBuckets = 1 << kLogBuckets;

struct Slot {
  uint64_t fingerprint : kHeadSize;
  uint64_t tail : kTailSize + 1;
  INLINE void Print() const {
    std::cout << "{" << std::hex << fingerprint << ", " << tail << "}";
  }
} __attribute__((packed));

static_assert(sizeof(Slot) == 2, "sizeof(Slot)");

struct Path : public Slot {
  uint64_t bucket;

  INLINE void Print() const {
    std::cout << "{" << std::dec << bucket << ", {" << std::hex << fingerprint << ", "
              << tail << "}}";
  }

  INLINE bool operator==(const Path& that) const {
    return bucket == that.bucket && fingerprint == that.fingerprint && tail == that.tail;
  }
};

INLINE Path ToPath(uint64_t raw, const Feistel& f, uint64_t log_side_size) {
  uint64_t pre_hash_index_and_fp = raw >> (64 - log_side_size - kHeadSize);
  uint64_t hashed_index_and_fp =
      f.Permute(log_side_size + kHeadSize, pre_hash_index_and_fp);
  uint64_t index = hashed_index_and_fp >> kHeadSize;
  Path p;
  p.bucket = index;
  p.fingerprint = hashed_index_and_fp;
  uint64_t pre_hash_index_fp_and_tail =
      raw >> (64 - log_side_size - kHeadSize - kTailSize);
  uint64_t raw_tail = pre_hash_index_fp_and_tail & ((1ul << kTailSize) - 1);
  uint64_t encoded_tail = raw_tail * 2 + 1;
  p.tail = encoded_tail;
  if (kDebug) {
    std::cout << std::hex << raw << " to ";
    p.Print();
    std::cout << " via " << std::dec << log_side_size + kHeadSize << " and " << std::hex
              << pre_hash_index_and_fp << std::endl;
  }
  return p;
}

INLINE uint64_t FromPathNoTail(Path p, const Feistel& f, uint64_t log_side_size) {
  uint64_t hashed_index_and_fp = (p.bucket << kHeadSize) | p.fingerprint;
  uint64_t pre_hashed_index_and_fp =
      f.ReversePermute(log_side_size + kHeadSize, hashed_index_and_fp);
  uint64_t shifted_up = pre_hashed_index_and_fp << (64 - log_side_size - kHeadSize);
  if (kDebug) {
    p.Print();
    std::cout << std::hex << " to " << shifted_up << " via " << std::dec
              << log_side_size + kHeadSize << std::endl;
  }
  return shifted_up;
}

struct Bucket {
  Slot data[kBuckets] = {};
  INLINE Slot& operator[](uint64_t x) { return data[x]; }
  INLINE const Slot& operator[](uint64_t x) const { return data[x]; }
};

static_assert(sizeof(Bucket) == sizeof(Slot) * kBuckets, "sizeof(Bucket)");

struct Side {
  Feistel f;
  Bucket* data;
  Path stash;

  INLINE Side(int log_side_size, const uint64_t* keys)
      : f(&keys[0]), data(new Bucket[1ul << log_side_size]()) {
    static_cast<Slot&>(stash) = {0, 0};
    stash.tail = 0;
  }
  INLINE ~Side() { delete[] data; }

  INLINE Bucket& operator[](unsigned i) { return data[i]; }
  INLINE const Bucket& operator[](unsigned i) const { return data[i]; }

  INLINE Path Insert(Path p, PcgRandom& rng) {
    assert(p.tail != 0);
    Bucket& b = data[p.bucket];
    for (int i = 0; i < kBuckets; ++i) {
      if (b[i].tail == 0) {
        b[i] = static_cast<Slot>(p);
        p.tail = 0;
        return p;
      }
      if (b[i].fingerprint == p.fingerprint && IsPrefixOf(b[i].tail, p.tail)) {
        return p;
      }
      // Path q;
      // static_cast<Slot&>(q) = b[i];
      // q.bucket = p.bucket;
      // assert(Find(q));
    }
    int i = rng.Get();
    Path result = p;
    static_cast<Slot&>(result) = b[i];
    b[i] = p;
    return result;
  }

  INLINE bool Find(Path p) const {
    if (kDebug) {
      std::cout << "Finding ";
      p.Print();
      std::cout << std::endl;
    }
    assert(p.tail != 0);
    if (kDebug && stash.tail != 0 && stash.bucket == p.bucket) {
      stash.Print();
      std::cout << std::endl;
    }
    if (p.bucket == stash.bucket && p.fingerprint == stash.fingerprint) return true;
    Bucket& b = data[p.bucket];
    for (int i = 0; i < kBuckets; ++i) {
      if (kDebug && b[i].tail != 0) {
        std::cout << "{" << p.bucket << ", {" << b[i].fingerprint << ", " << b[i].tail
                  << "}}" << std::endl;
      }
      // if (b[i].tail == 0) break;
      if (b[i].fingerprint == p.fingerprint && IsPrefixOf(b[i].tail, p.tail)) {
        return true;
      }
    }
    return false;
  }
};

}  // namespace detail
}  // namespace filter

namespace std {
INLINE void swap(filter::detail::Side& x, filter::detail::Side& y) {
  using std::swap;
  swap(x.f, y.f);
  auto* tmp = x.data;
  x.data = y.data;
  y.data = tmp;
  swap(x.stash, y.stash);
}
}  // namespace std

namespace filter {

struct ElasticFilter {
  INLINE static const char* Name() {
    thread_local const constexpr char result[] = "Elastic";
    return result;
  }
  detail::Side left, right;
  uint64_t log_side_size;
  detail::PcgRandom rng = {detail::kLogBuckets};
  const uint64_t* entropy;
  uint64_t occupied = 0;

  INLINE uint64_t SizeInBytes() const {
    return 2 * (sizeof(detail::Path) +
                sizeof(detail::Slot) * (1 << log_side_size) * detail::kBuckets);
  }

  INLINE uint64_t Count() const {
    uint64_t result = 0;
    for (auto s : {&left, &right}) {
      if (s->stash.tail != 0) ++result;
      for (uint64_t i = 0; i < 1ull << log_side_size; ++i) {
        for (int j = 0; j < detail::kBuckets; ++j) {
          if ((*s)[i][j].tail != 0) ++result;
        }
      }
    }
    return result;
  }

  INLINE void Print() const {
    for (auto& s : {&left, &right}) {
      s->stash.Print();
      std::cout << std::endl;
      for (uint64_t i = 0; i < 1ull << log_side_size; ++i) {
        for (int j = 0; j < detail::kBuckets; ++j) {
          (*s)[i][j].Print();
          std::cout << std::endl;
        }
      }
    }
  }

  INLINE ElasticFilter(int log_side_size, const uint64_t* entropy)
      : left(log_side_size, entropy),
        right(log_side_size, entropy + 6),
        log_side_size(log_side_size),
        entropy(entropy) {}

  INLINE static ElasticFilter CreateWithBytes(uint64_t bytes) {
    thread_local constexpr const uint64_t kEntropy[13] = {
        0x2ba7538ee1234073, 0xfcc3777539b147d6, 0x6086c563576347e7, 0x52eff34ee1764465,
        0x8639cbf57f264867, 0x5a31ee34f0224ccb, 0x07a1cb8140744ee6, 0xf2296cf6a6524e9f,
        0x28a31cec9f6d4484, 0x688f3fe9de7245f6, 0x1dc17831966b41a2, 0xf227166e425e4b0c,
        0x15ab11b1a6bf4ea8};
    return ElasticFilter(0, kEntropy);
    return ElasticFilter(log(bytes / 2 / detail::kBuckets) / log(2), kEntropy);
  }

  INLINE bool FindHash(uint64_t k) const {
    return left.Find(detail::ToPath(k, left.f, log_side_size)) ||
           right.Find(detail::ToPath(k, right.f, log_side_size));
  }

  enum class InsertResult { Ok, Stashed, Failed };

  INLINE InsertResult InsertHash(uint64_t k) {
    if (occupied > 0.95 * (1ul << log_side_size) * 2 * detail::kBuckets) {
      Upsize();
    }
    auto result = Insert(detail::ToPath(k, left.f, log_side_size));
    // if (result == InsertResult::Ok) assert(Find(k));
    return result;
  }

  INLINE InsertResult Insert(detail::Path q) {
    int ttl = 32;
    // assert(occupied == Count());
    while (left.stash.tail != 0 && right.stash.tail != 0) {
      ttl = 2 * ttl;
      detail::Path p = left.stash;
      if (detail::kDebug) {
        std::cout << "Unstash L ";
        p.Print();
        std::cout << std::endl;
      }
      left.stash.tail = 0;
      --occupied;
      InsertResult result = Insert(p, ttl);
      assert(result != InsertResult::Failed);
      if (result == InsertResult::Ok) break;

      p = right.stash;
      if (detail::kDebug) {
        std::cout << "Unstash R ";
        p.Print();
        std::cout << std::endl;
      }
      uint64_t tail = p.tail;
      right.stash.tail = 0;
      --occupied;
      // p = ToPath(FromPathNoTail(left.stash, left.f, log_side_size), right.f,
      //            log_side_size);
      p = detail::ToPath(detail::FromPathNoTail(p, right.f, log_side_size), left.f,
                         log_side_size);
      p.tail = tail;
      result = Insert(p, ttl);
      assert(result != InsertResult::Failed);
      if (result == InsertResult::Ok) break;
      // assert(occupied == Count());
      if (detail::kDebug) {
        Print();
        std::cout << "ttl " << std::dec << ttl << " with " << occupied << " / "
                  << (2 * detail::kBuckets << log_side_size) << std::endl;
      }
    }
    // assert(occupied == Count());
    return Insert(q, 1);
  }

  // TODO: what if already present?
  INLINE InsertResult Insert(detail::Path p, int ttl) {
    // assert(occupied == Count());
    if (detail::kDebug) {
      std::cout << "Insert ";
      p.Print();
      std::cout << std::endl;
    }
    if (left.stash.tail != 0 && right.stash.tail != 0) return InsertResult::Failed;
    uint64_t tail = p.tail;
    while (true) {
      detail::Path q = p;
      p = left.Insert(p, rng);
      // assert(left.Find(q));
      if (p.tail == 0) {
        ++occupied;
        if (detail::kDebug) std::cout << "Done" << std::endl;
        // assert(occupied == Count());
        return InsertResult::Ok;
      }
      if (p == q) {
        return InsertResult::Ok;
      }
      tail = p.tail;
      if (ttl <= 0 && left.stash.tail == 0) {
        left.stash = p;
        ++occupied;
        if (detail::kDebug) {
          std::cout << "stashed R ";
          p.Print();
          std::cout << std::endl;
        }
        // assert(occupied == Count());
        return InsertResult::Stashed;
      }
      // p.Print();

      auto temp = detail::FromPathNoTail(p, left.f, log_side_size);
      // std::cout << temp << std::endl;
      p = detail::ToPath(temp, right.f, log_side_size);
      // p.Print();
      p.tail = tail;
      q = p;
      p = right.Insert(p, rng);
      if (p.tail == 0) {
        ++occupied;
        if (detail::kDebug) std::cout << "Done" << std::endl;
        // assert(occupied == Count());
        return InsertResult::Ok;
      }
      if (p == q) {
        return InsertResult::Ok;
      }
      tail = p.tail;
      if (ttl <= 0) {
        assert(right.stash.tail == 0);
        right.stash = p;
        ++occupied;
        if (detail::kDebug) {
          std::cout << "stashed R ";
          p.Print();
          std::cout << std::endl;
        }
        // assert(occupied == Count());
        return InsertResult::Stashed;
      }
      --ttl;
      p = detail::ToPath(detail::FromPathNoTail(p, right.f, log_side_size), left.f,
                         log_side_size);
      p.tail = tail;
    }
  }

  INLINE void Upsize();
};

}  // namespace filter

namespace std {
INLINE void swap(filter::ElasticFilter& x, filter::ElasticFilter& y) {
  using std::swap;
  swap(x.left, y.left);
  swap(x.right, y.right);
  swap(x.log_side_size, y.log_side_size);
  swap(x.rng, y.rng);
  swap(x.entropy, y.entropy);
  swap(x.occupied, y.occupied);
}
}  // namespace std

namespace filter {
namespace detail {
INLINE void UpsizeHelper(Slot sl, uint64_t i, ElasticFilter* u, Side ElasticFilter::*s,
                         ElasticFilter& t) {
  if (sl.tail == 0) return;
  detail::Path p;
  static_cast<Slot&>(p) = sl;
  p.bucket = i;
  // assert(t.occupied == t.Count());
  uint64_t q = FromPathNoTail(p, (u->*s).f, u->log_side_size);
  if (sl.tail == 1ul << kTailSize) {
    p = ToPath(q, (t.left).f, t.log_side_size);
    p.tail = sl.tail;
    t.Insert(p);
    q |= (1ul << (64 - u->log_side_size - kHeadSize - 1));
    p = ToPath(q, (t.left).f, t.log_side_size);
    p.tail = sl.tail;
    t.Insert(p);
  } else {
    q |= static_cast<uint64_t>(sl.tail >> kTailSize)
         << (64 - u->log_side_size - kHeadSize - 1);
    detail::Path r = ToPath(q, (t.left).f, t.log_side_size);
    r.tail = (sl.tail << 1);
    // p.Print();
    // std::cout << std::hex << q << std::endl;
    // r.Print();
    t.Insert(r);
    // r.Print();
    // std::cout << std::endl;
  }
  // assert(t.occupied == t.Count());
}
}  // namespace detail

INLINE void ElasticFilter::Upsize() {
  // assert(occupied == Count());
  // std::cout << "Upsize " << log_side_size << std::endl;
  ElasticFilter t(1 + log_side_size, entropy);
  for (auto s : {&ElasticFilter::left, &ElasticFilter::right}) {
    detail::Path stash = (this->*s).stash;
    if (stash.tail != 0) {
      UpsizeHelper(stash, stash.bucket, this, s, t);
    }
    for (unsigned i = 0; i < (1u << log_side_size); ++i) {
      for (int j = 0; j < detail::kBuckets; ++j) {
        detail::Slot sl = (this->*s)[i][j];
        detail::UpsizeHelper(sl, i, this, s, t);
      }
    }
  }
  using std::swap;
  // assert(t.occupied == t.Count());
  swap(*this, t);
  // assert(occupied == Count());
  // std::cout << "Done upsize " << log_side_size << std::endl;
}

}  // namespace filter
