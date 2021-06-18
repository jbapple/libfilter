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

namespace {

#if 0
thread_local const constexpr int kLogLevels = 5;
thread_local const constexpr uint64_t kLevels = 1ul << kLogLevels;

// From the paper, kTailSize is the log of the number of times the size will
// double, while kHead size is two more than ceil(log(1/epsilon)) + i, bit i is only used
// up  by 1 (for the sides) plus kLogBuckets. For instance, with kTailSize = 5 and
// kHeadSize = 10, log(1/epsilon) is 10 - 2 - kLogBuckets - 1, which is an fpp of about
// 3%. Yikes! The good news is that checking against the tails and in the early stages of
// growth, epsilon is lower. TODO: how much lower, in theory?
//
// Note that kHeadSize must be large enough for quotient cuckoo hashing to work sensibly:
// it needs some randomness in the fingerprint to ensure each item hashes to sufficiently
// different buckets kHead is just the rest of the uint16_t, and is log2(1/epsilon)
thread_local const constexpr int kHeadSize = 8;
thread_local const constexpr int kTailSize = 7;
static_assert(kHeadSize + kTailSize == 15, "kHeadSize + kTailSize == 15");

// The number of slots in each cuckoo table bucket. The higher this is, the easier it is
// to do an insert but the higher the fpp.
thread_local const constexpr int kLogBuckets = 2;
thread_local const constexpr int kBuckets = 1 << kLogBuckets;

struct Slot {
  bool long_fp : 1;
  uint64_t fingerprint : kHeadSize;
  uint64_t tail : kTailSize +
                  1;  // +1 for the encoding of sequences above. tail == 0 indicates an
                      // empty slot, no matter what's in fingerprint
  INLINE void Print() const {
    std::cout << "{" << long_fp << std::hex << fingerprint << ", " << tail << "}";
  }
} __attribute__((packed));

static_assert(sizeof(Slot) == 2, "sizeof(Slot)");

// A path encodes the slot number, as well as the slot itself.
struct Path : public Slot {
  uint64_t level : kLogLevels;
  uint64_t bucket;

  INLINE void Print() const {
    std::cout << "{" << std::dec << level << bucket << ", {" << long_fp << std::hex << fingerprint << ", "
              << tail << "}}";
  }

  INLINE bool operator==(const Path& that) const {
    return level == that.level && bucket == that.bucket && long_fp == that.long_fp &&
           fingerprint == that.fingerprint && tail == that.tail;
  }


};


// Converts a hash value to a path. The bucket and the fingerprint are acquired via
// hashing, while the tail is a selection of un-hashed bits from raw. Later, when moving
// bits from the tail to the fingerprint, one must un-hash the bucket and the fingerprint,
// move the bit, then re-hash
//
// Operates on the high bits, since bits must be moved from the tail into the fingerprint
// and then bucket.
//
/*

A raw key can be made into up to four different paths, one or two per side. In each side
there are two hash functions for differen traw input lengths. The one for long input
length always yields a path, though what "shape" path it yields depends on the cursor and
the level. If the level is less than the cursor, then the fingerprint is short and the
index is long. Mutatis mutandis.

The one for short input yields a path iff the level is greater than or equal to the
cursor.


*/
/*
INLINE void ToPaths(uint64_t raw, const Feistel f[2], uint64_t log_small_size,
                    uint64_t cursor, Path result[2]) {
  for (unsigned delta : {1, 0}) {
    uint64_t pre_hash_level_index_and_fp =
        raw >> (64 - log_small_size - delta - kHeadSize - kLogLevels);
    uint64_t hashed_level_index_and_fp = hi.Permute(
        log_side_size + delta + kHeadSize + kLogLevels, pre_hash_level_index_and_fp);
    uint64_t level = hashed_level_index_and_fp >> (kHeadSize + log_side_size + delta);
    if (level < cursor && delta == 0) {
      result[i].tail = 0;
      continue;
    }
    uint64_t index = Mask(log_side_size + (level < cursor), hashed_level_index_and_fp >> kHeadSize);
    result[i].long_fp = (level >= cursor) && (delta > 0);
    result[i].level = level;
    result[i].bucket = index;
    // Helpfully gets cut off by being a bitfield
    result[i].fingerprint = hashed_level_index_and_fp;
    uint64_t pre_hash_level_index_fp_and_tail =
        raw >> (64 - log_side_size - delta - kHeadSize - kTailSize - kLogLevels);
    uint64_t raw_tail = Mask(kTailSize, pre_hash_level_index_fp_and_tail);
    // encode the tail using the encoding described above, in which the length of the tail
    // i the complement of the tzcnt plus one.
    uint64_t encoded_tail = raw_tail * 2 + 1;
    result[i].tail = encoded_tail;
  }
}
*/

/*
INLINE constexpr Path ToPath(uint64_t raw, const Feistel f, uint64_t level_size,
                             uint64_t fingerprint_size) {
  const uint64_t pre_hash_level_index_fp_and_tail =
      raw >> (64 - kLogLevels - level_size - fingerprint_size - kTailSize);
  const uint64_t pre_hash_level_index_and_fp =
      pre_hash_level_index_fp_and_tail >> kTailSize;
  const uint64_t hashed_level_index_and_fp =
      hi.Permute(kLogLevels + level_size + fingerprint_size, pre_hash_level_index_and_fp);
  const uint64_t level = hashed_level_index_and_fp >> (level_size + fingerprint_size);
  const uint64_t level_index = Mask(level_size, hashed_level_index_and_fp >> fingerprint_size);
  result[i].level = level;
  result[i].bucket = level_index;
  result[i].long_fp = fingerprint_size >= kHeadSize;
  // Helpfully gets cut off by being a bitfield
  result[i].fingerprint = hashed_level_index_and_fp;
  const uint64_t raw_tail = Mask(kTailSize, pre_hash_level_index_fp_and_tail);
  // encode the tail using the encoding described above, in which the length of the tail
  // i the complement of the tzcnt plus one.
  result[i].tail =  raw_tail * 2 + 1;
}
*/

enum class SlotType { kLongIndex = 0, kLongFingerprint = 1, kShort = 2 };

// paths are either long or short. long paths are either before the cursor with long_fp
// false or after with long_fp true. short paths are after the cursor with long_fp false.
//
// There are no valid paths before the cursor with long_fp true.
//
// if full_is_short is true and the path is before the cursor, this function retuens an
// empty path (tail == 0)
INLINE constexpr Path ToPath(uint64_t raw, const Feistel f, int cursor,
                             uint64_t low_level_size, bool full_is_short) {
  const uint64_t pre_hash_level_index_fp_and_tail =
      raw >> (64 - kLogLevels - low_level_size - kHeadSize + full_is_short - kTailSize);
  const uint64_t raw_tail = Mask(kTailSize, pre_hash_level_index_fp_and_tail);
  const uint64_t pre_hash_level_index_and_fp =
    pre_hash_level_index_fp_and_tail >> kTailSize;
  const uint64_t hashed_level_index_and_fp =
      f.Permute(kLogLevels + low_level_size + kHeadSize - full_is_short, pre_hash_level_index_and_fp);
  Path result;
  result.level = hashed_level_index_and_fp >> (low_level_size + kHeadSize - full_is_short);
  bool big_index = (level < cursor);
  if (big_index && full_is_short) {
    result.tail = 0;
    return result;
  }

  result[i].bucket =
      Mask(low_level_size + big_index,
           hashed_level_index_and_fp >> (kHeadSize - full_is_short - big_index));
  result[i].long_fp = not big_index && not full_is_short;

  result[i].fingerprint =
      Mask(kHeadSize - full_is_short - big_index, hashed_level_index_and_fp);

  // encode the tail using the encoding described above, in which the length of the tail
  // i the complement of the tzcnt plus one.
  result[i].tail =  raw_tail * 2 + 1;
  return result;
}

// Uses reverse permuting to get back the high bits of the original hashed value. Elides
// the tail, since the tail may have a limited length, and once that's appended to a raw
// value, one can't tell a short tail from one that just has a lot of zeros at the end.
INLINE constexpr uint64_t FromPathNoTail(Path p, const Feistel& f, uint64_t level_size,
                                         uint64_t fingerprint_size) {
  const uint64_t hashed_level_index_and_fp =
      (((p.level << level_size) | p.bucket) << fingerprint_size) | p.fingerprint;
  const uint64_t pre_hashed_index_and_fp = f.ReversePermute(
      kLogLevels + level_size + fingerprint_size, hashed_level_index_and_fp);
  uint64_t shifted_up = pre_hashed_index_and_fp
                        << (64 - kLogLevels - level_size - fingerprint_size);
  return shifted_up;
}

#endif // 0

}
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

INLINE Path RePath(Path p, const Feistel from_short, const Feistel from_long,
                   const Feistel to_short, const Feistel to_long, uint64_t log_small_size,
                   uint64_t from_cursor, uint64_t to_cursor, Path* out) {
  if (p.level < from_cursor) {
    assert(not p.long_fp);
    const uint64_t key = FromPathNoTail(p, from_long, log_small_size + 1, kHeadSize - 1);
    Path q = ToPath(key, to_long, log_small_size, false /* full is not short */);
    q.tail = p.tail;
    out->tail = 0;
    return q;
  }
  if (p.long_fp) {
    const uint64_t key = FromPathNoTail(p, from_long, log_small_size, kHeadSize);
    Path q = ToPath(key, to_long, log_small_size, false);
    q.tail = p.tail;
    out->tail = 0;
    return q;
  }
  uint64_t key = FromPathNoTail(p, from_short, log_small_size, kHeadSize - 1);
  Path q = ToPath(key, to_short, to_cursor, log_small_size,
                  true /* full is short because tail is short */);
  q.tail = p.tail;
  if (q.level >= to_cursor) return q;
  // q is invalid - the level is low, but there aren't enough bits for the fingerprint
  if (p.tail != 1 << kTailSize) {
    uint64_t k =
        key | ((p.tail >> kTailSize) << (64 - kLogLevels - log_small_size - kHeadSize));
    Path q2 = ToPath(k, to_long, log_small_size,
                     false /* full is now long, since we added a bit to k*/);
    q2.tail = p.tail << 1;
    out->tail = 0;
    return q2;
  }
  // p.tail is empty!
  *out = ToPath(key, to_long, log_small_size, false);
  uint64_t k = key | (1ul << (64 - kLogLevels - log_small_size - kHeadSize));
  Path q2 = ToPath(k, to_long, log_small_size, false);
  q2.tail = p.tail;
  return q2;
}

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









































#if 0

  INLINE void InsertHash(uint64_t k) {
    // 95% is achievable, generally,but give it some room
    if (occupied > 0.94 * Capacity())
      Upsize();
    }
    // TODO: only need one path here
    Paths p[2];
    detail::ToPath(k, {sides[s].lo, sides[s].hi}, log_side_size, cursor, p);
    Insert(0, p[0]);
  }

 protected:
  // After Stashed result, HT is close to full and should be upsized
  enum class InsertResult { Ok, Stashed, Failed };

  // After ttl, stash the input and return Stashed. Pre-condition: at least one stash is
  // empty. Also, p is a left path, not a right one.
  INLINE InsertResult Insert(int s, detail::Path p, int ttl) {
    if (sides[0].stash.tail != 0 && sides[1].stash.tail != 0) return InsertResult::Failed;
    detail::Side* both[2] = {&sides[s], &sides[1 - s]};
    while (true) {
#if defined(__clang)
#pragma unroll
#else
#pragma GCC unroll 2
#endif
      for (int i = 0; i < 2; ++i) {

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
        auto tail = p.tail;
        if (ttl <= 0 && both[i]->stash.tail == 0) {
          // we've run out of room. If there's room in this stash, stash it here. If there
          // is not room in this stash, there must be room in the other, based on the
          // pre-condition for this method.
          both[i]->stash = p;
          ++occupied;
          return InsertResult::Stashed;
        }
        --ttl;
        /*

          If p has a long fingerprint, unhash it and rehash it to the other side. Long
          fingerprints are only in the section of the structure greater than the
          cursor. If the level hasn't changed which side of the cursor it is on, proceed.
          Otherwise, the level now has more slots. Luckily, we have enough room to
          accomate those by shifting a bit from the re-hashed fingerprint to the re-hashed
          index.
        */

        /*
          If p has a short fingerprint and is on the side of the cursor with more slots,
          then unhash and rehash like above. Now, if the rehashed result is on the other
          side of the cursor, simply steal a bit from the rehashed index and put it in the
          rehashed fingerprint.

          Otherwise, p has a short fingerprint and is in the small side of the cursor. Now
          if the rehash produces a value in the small side of the cursor, we are again
          fine, but producing a value on the big side of he cursor is a problem. Here we
          have to steal bits from the pre-hashed tail to put in the pre-hashed fingerprint
          (really just the about-to-be-hashed part of the key). ANd if there are no bits
          to steal, the raw input must produce two paths, one each with a new manufactured
          bit from where the tail would be.

         */
        Path extra;
        Path next = RePath(p, both[i]->lo, both[i]->hi, both[1 - i]->lo, both[1 - i]->hi,
                           log_side_size, cursor, &extra);
        if (extra.tail != 0) Insert(1 - s, extra, ttl - 1);
        // TODO: what if insert returns stashed? Do we need multiple states? Maybe green , yellow red?
        // Or maybe break at the beginning of this logic if repath returns two things
        // or retry if there aren't two stashes open at this time.
        p = next;


      }
    }
  }

  // This method just increases ttl until insert succeeds.
  // TODO: upsize when insert fails with high enough ttl?
  INLINE InsertResult Insert(int s, detail::Path q) {
    int ttl = 32;
    //If one stash is empty, we're fine. Only go here if both stashes are full
    while (sides[0].stash.tail != 0 && sides[1].stash.tail != 0) {
      ttl = 2 * ttl;
#if defined(__clang)
#pragma unroll
#else
#pragma GCC unroll 2
#endif
      for (int i = 0; i < 2; ++i) {
        int t = s^i;
        // remove stash value
        detail::Path p = sides[t].stash;
        sides[t].stash.tail = 0;
        --occupied;
        // Insert it. We know this will succeed, as one stash is now empty, but we hope it
        // succeeds with Ok, not Stashed.
        InsertResult result = Insert(t, p, ttl);
        assert(result != InsertResult::Failed);
        // At least one stash is now free. Can now do the REAL insert (after the loop)
        if (result == InsertResult::Ok) break;
      }
    }
    // Can totally punt here. IOW, the stashes are ALWAYS full, since we don't even try
    // very hard not to fill them!
    return Insert(s, q, 1);
  }

#endif  // 0
