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

#include "util.hpp"
#include "paths.hpp"

namespace filter {

namespace detail {

/*

For context, let the cursor be c, the log_small_size be m, kHeadSize be k, and kLogLevels
be L.

Each side has two permutations. The permutations operate on bitstrings of length k and
k+1. Each key has to be checked in up to four places:

1. The path in side 0 formed by permuting the top L+m+k bits
2. The path in side 0 formed by permuting the top L+m+k-1 bits
3. The path in side 1 formed by permuting the top L+m+k bits
4. The path in side 1 formed by permuting the top L+m+k-1 bits

When we encounter a path in the table, it was produced in one of three ways.

A. If the level has index less than c, then the level has length 1 << (m+1) and the
fingerprint has length k-1. In this case, the slot was produced from the key by permuting
the top L+m+k bits.

B. The same is true of slots with fingerprints of length k and level indexes greater than
or equal to c.

C. For slots with fingerprints of length k-1 and level indexes greater than or equal to c,
the slot was produced from the key by permuting the top L+m+k-1 bits.

To change a path to another one representing the same key

 * Reverse permute the L+m+k bits, then permute them with the larger permutation from the
   other side. Split this appropriately into parts based on the size of the slot array at
   the level in question.

 * Reverse permute the L+m+k-1 bits. Permute them with the small permutation from the
   other side. If the resulting level is greater than or equal to than c, then resplit the
   path appropriately. Otherwise, we say rehashing with the small permuation failed. We
   now need to transmute to the other case in which the larger permutation is used.
   |
   To do so, we need to extend the key produced by the reverse permutation. Here, too,
   there are two cases.
   |
   If the tail is non-empty, we can move a bit from it, thus shortening it, and then
   permute with the large permutation from the other side.
   |
   Otherwise, we form two longer keys by setting the low bit to 0 and to 1. These two are
   then treated as longer keys and permuted with the larger permuation from the other
   side.

*/


struct Bucket {
  Slot data[kBuckets] = {};
  INLINE Slot& operator[](uint64_t x) { return data[x]; }
  INLINE const Slot& operator[](uint64_t x) const { return data[x]; }
};

static_assert(sizeof(Bucket) == sizeof(Slot) * kBuckets, "sizeof(Bucket)");

struct Level {
  Bucket* data;

  INLINE Level(int log_level_size)
      : data(new Bucket[1ul << log_side_size]()) {
  }
  INLINE ~Level() { delete[] data; }

  INLINE Bucket& operator[](unsigned i) { return data[i]; }
  INLINE const Bucket& operator[](unsigned i) const { return data[i]; }

  // Returns an empty path (tail = 0) if insert added a new element. Returns p if insert
  // succeded without anning anything new. Returns something else if that something else
  // was displaced by the insert. That item must be inserted then
  INLINE Path Insert(Path p, PcgRandom& rng) {
    assert(p.tail != 0);
    Bucket& b = data[p.bucket];
    for (int i = 0; i < kBuckets; ++i) {
      if (b[i].tail == 0) {
        // empty slot
        b[i] = static_cast<Slot>(p);
        p.tail = 0;
        return p;
      }
      if (b[i].long_fp == p.long_fp && b[i].fingerprint == p.fingerprint) {
        // already present in the table
        if (IsPrefixOf(b[i].tail, p.tail)) {
          return p;
        }
        /*
        // This has a negligible effect on fpp
        auto c = Combinable(b[i].tail, p.tail);
        if (c > 0) {
          b[i].tail = c;
          return p;
        }
        */
      }
    }
    // Kick something random and return it
    int i = rng.Get();
    Path result = p;
    static_cast<Slot&>(result) = b[i];
    b[i] = p;
    return result;
  }

  INLINE bool Find(Path p) const {
    assert(p.tail != 0);
    Bucket& b = data[p.bucket];
    for (int i = 0; i < kBuckets; ++i) {
      if (b[i].tail == 0) continue;
      if (b[i].long_fp == p.long_fp && b[i].fingerprint == p.fingerprint &&
          IsPrefixOf(b[i].tail, p.tail)) {
        return true;
      }
    }
    return false;
  }

  friend void swap(Level&, Level&);
};

friend void swap(Level& x, Level& y) {
  using std::swap;
  swap(x.data, y.data);
}

// A cuckoo hash table is made up of sides (in some incarnations), rather than jut two
// buckets from one array. Each side has a stash that holds any paths that couldn't fit.
// This is useful for random-walk cuckoo hashing, in which the leftover path needs  place
// to be stored so it doesn't invalidate old inserts.
struct Side {
  Feistel hi, lo;
  Level levels[kLevels];
  Path stash;

  INLINE Side(int log_level_size, const uint64_t* keys)
    : hi(&keys[0]), lo(&keys[6]) {
    for (uint64_t i = 0; i < kLevels; ++i) {
      new (&levels[i]) Level(log_level_size);
    }
    stash.tail = 0;
  }
  INLINE ~Side() {
    for (uint64_t i = 0; i < kLevels; ++i) {
      levels[i].~Level();
    }
  }

  INLINE Level& operator[](unsigned i) { return levels[i]; }
  INLINE const Level& operator[](unsigned i) const { return levels[i]; }

  // Returns an empty path (tail = 0) if insert added a new element. Returns p if insert
  // succeded without anning anything new. Returns something else if that something else
  // was displaced by the insert. That item must be inserted then
  INLINE Path Insert(Path p, PcgRandom& rng) {
    return levels[p.level].Insert(p, rng);
  }

  INLINE bool Find(Path p) const {
    return levels[p.level].Find(p);
  }

  friend void swap(Side&, Side&);
};

INLINE void swap(Side& x, Side& y) {
  using std::swap;
  swap(x.hi, y.hi);
  swap(x.lo, y.lo);
  for (uint64_t i = 0; i < kLevels; ++i) {
    swap(x.levels[i], y.levels[i]);
  }
  swap(x.stash, y.stash);
}

}  // namespace

struct MinimalPlasticFilter {
  INLINE static const char* Name() {
    thread_local const constexpr char result[] = "MinPlastic";
    return result;
  }

 protected:
  detail::Side sides[2];
  uint64_t cursor;
  uint64_t log_side_size;
  detail::PcgRandom rng = {detail::kLogBuckets};
  const uint64_t* entropy;

 public:
  uint64_t occupied = 0;

  MinimalPlasticFilter(MinimalPlasticFilter&&);
  /*
  ElasticFilter(const ElasticFilter& that)
      : sides{{(int)that.log_side_size, that.entropy},
              {(int)that.log_side_size, that.entropy + 12}},
        cursor(0),
        log_side_size(that.log_side_size),
        rng(that.rng),
        entropy(that.entropy),
        occupied(that.occupied) {
    for (int i = 0; i < 2; ++i) {
      memcpy(sides[i].data, that.sides[i].data,
             sizeof(detail::Bucket) << that.log_side_size);
    }
  }
  */
  /*
  ElasticFilter& operator=(const ElasticFilter& that) {
    this->~ElasticFilter();
    new (this) ElasticFilter(that);
    return *this;
  }
  */

  uint64_t Capacity() const {
    return 2 + 2 * detail::kBuckets *
                   ((1ul << log_side_size) * detail::kLevels +
                    (1ul << log_side_size) * cursor);
  }

  uint64_t SizeInBytes() const { sizeof(detail::Path) * Capacity(); }

 protected:
  // Verifies the occupied field:
  /*
  uint64_t Count() const {
    uint64_t result = 0;
    for (int s = 0; s < 2; ++s) {
      if (sides[s].stash.tail != 0) ++result;
      for (uint64_t i = 0; i < 1ull << log_side_size; ++i) {
        for (int j = 0; j < detail::kBuckets; ++j) {
          if (sides[s][i][j].tail != 0) ++result;
        }
      }
    }
    return result;
  }
  */
 public:
  /*
  void Print() const {
    for (int s = 0; s < 2; ++s) {
      sides[s].stash.Print();
      std::cout << std::endl;
      for (uint64_t i = 0; i < 1ull << log_side_size; ++i) {
        for (int j = 0; j < detail::kBuckets; ++j) {
          sides[s][i][j].Print();
          std::cout << std::endl;
        }
      }
    }
  }
  */
  MinimalPlasticFilter(int log_side_size, const uint64_t* entropy)
      : sides{{log_side_size, entropy}, {log_side_size, entropy + 6}},
        cusrsor(0),
        log_side_size(log_side_size) {}

  static MinimalPlasticFilter CreateWithBytes(uint64_t bytes) {
    thread_local constexpr const uint64_t kEntropy[24] = {
        0x2ba7538ee1234073, 0xfcc3777539b147d6, 0x6086c563576347e7, 0x52eff34ee1764465,
        0x8639cbf57f264867, 0x5a31ee34f0224ccb, 0x07a1cb8140744ee6, 0xf2296cf6a6524e9f,
        0x28a31cec9f6d4484, 0x688f3fe9de7245f6, 0x1dc17831966b41a2, 0xf227166e425e4b0c,
        0x4a2a62bafc694440, 0x2e6bbea775e3429d, 0x5687dd060ba64169, 0xc5d95e8a38a44789,
        0xd30480ab74084edc, 0xd72483670ec14df3, 0x0414954940374787, 0x8cd86adfda93493f,
        0x50d61c3272a24ccb, 0x40cb1e4f0da34cc3, 0xb88f09c3af35472e, 0x8de6d01bb8a849a5};

    return MinimalPlasticFilter(std::max(1.0, log(1.0 * bytes / 2 / detail::kBuckets /
                                           sizeof(detail::Slot) / detail::kBuckets) /
                                           log(2)),
                         kEntropy);
   }

  INLINE bool FindHash(uint64_t k) const {
    INLINE constexpr Path ToPath(uint64_t raw, const Feistel f, int cursor,
                                 uint64_t low_level_size, bool full_is_short);
    for (int i : {0,1}) {
      if (sides[i].Find(ToPath(k, sides[i].lo, cursor, log_side_size, true))) {
        return true;
      }
      if (sides[i].Find(ToPath(k, sides[i].hi, cursor, log_side_size, false))) {
        return true;
      }
    }
    return false;
  }

  INLINE void InsertHash(uint64_t k) {
    if (occupied > 0.94 * Capacity()) {
      Upsize();
    }
    // TODO: only need one path here
    Path p = detail::ToPath(k, sides[s].hi, cursor, log_side_size, false);
    Insert(p);
  }

  INLINE void Insert(int side, Path p) {
    while (true) {
      for (int i : {side, 1 - side}) {
        detail::Path q = p;
        p = both[i]->Insert(p, rng);
        if (p.tail == 0) {
          // Found an empty slot
          ++occupied;
          return InsertResult::Ok;
        }
        if (p == q) {
          // Combined with or already present in a slot. Success, but no increase in
          // filter size
          return InsertResult::Ok;
        }
        Path extra;
        Path next = RePath(p, both[i]->lo, both[i]->hi, both[1 - i]->lo, both[1 - i]->hi,
                           log_side_size, cursor, cursor, &extra);
        if (extra.tail != 0) Insert(1 - i, extra);
        // TODO: what if insert returns stashed? Do we need multiple states? Maybe green , yellow red?
        // Or maybe break at the beginning of this logic if repath returns two things
        // or retry if there aren't two stashes open at this time.
        p = next;
      }
    }
  }


  friend void swap(MinimalPlasticFilter&, MinimalPlasticFilter&);

  // Double the size of one level of the filter
  void Upsize() {
    ++cursor;
    Slot* next[2];
    next[0] = new Slot[2 << log_side_size]();
    next[1] = new Slot[2 << log_side_size]();
    Slot* last[2] = {sides[0][cursor], sides[1][cursor]};
    sides[0][cursor] = next[0];
    sides[1][cursor] = next[1];
    Path p;
    p.level = cursor;
    for (int s : {0, 1}) {
      for (unsigned i = 0; i < (1 << log_side_size); ++i) {
        if (last[i].tail == 0) continue;
        p.bucket = i;
        static_cast<Slot>(p) = last[i];
        Path q;
        Path r = RePath(p, sides[s]->lo, sides[s]->hi, sides[s]->lo, sides[s]->hi,
                        log_side_size, cursor - 1, cursor, &q);
        if (q.tail != 0) Insert(s, q);
        Insert(s, r);
      }
    }
    if (cursor = kLevels) {
      cursor = 0;
      ++log_side_size;
      using namespace std;
      for (int i : {0, 1}) swap(sides[i].lo, sides[i].hi);
    }
    delete[] last[0];
    delete[] last[1];
  }
};

INLINE void swap(MinimalPlasticFilter& x, MinimalPlasticFilter& y) {
  using std::swap;
  swap(x.sides[0], y.sides[0]);
  swap(x.sides[1], y.sides[1]);
  swap(x.log_side_size, y.log_side_size);
  swap(x.rng, y.rng);
  swap(x.entropy, y.entropy);
  swap(x.occupied, y.occupied);
}

}  // namespace filter
