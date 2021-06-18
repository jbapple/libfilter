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

namespace filter {

namespace detail {

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
thread_local const constexpr int kHeadSize = 7;
thread_local const constexpr int kTailSize = 7;
static_assert(kHeadSize + kTailSize == 14, "kHeadSize + kTailSize == 14");

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
  constexpr Slot() :long_fp(), fingerprint(), tail() {}
} __attribute__((packed));

static_assert(sizeof(Slot) == 2, "sizeof(Slot)");

// A path encodes the slot number, as well as the slot itself.
struct Path : public Slot {
  uint64_t level : kLogLevels;
  uint64_t bucket;

  constexpr Path(): Slot(), level(), bucket() {}
  
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
// empty path (tail == 0).
INLINE constexpr Path ToPath(uint64_t raw, const Feistel& f, int cursor,
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
  bool big_index = (result.level < cursor);
  if (big_index && full_is_short) {
    result.tail = 0;
    return result;
  }

  result.bucket =
      Mask(low_level_size + big_index,
           hashed_level_index_and_fp >> (kHeadSize - full_is_short - big_index));
  result.long_fp = not big_index && not full_is_short;

  result.fingerprint =
      Mask(kHeadSize - full_is_short - big_index, hashed_level_index_and_fp);

  // encode the tail using the encoding described above, in which the length of the tail
  // i the complement of the tzcnt plus one.
  result.tail =  raw_tail * 2 + 1;
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

INLINE constexpr Path RePath(Path p, const Feistel& from_short, const Feistel& from_long,
                             const Feistel& to_short, const Feistel& to_long,
                             uint64_t log_small_size, uint64_t cursor, Path* out) {
  if (p.level < cursor) {
    const uint64_t key = FromPathNoTail(p, from_long, log_small_size + 1, kHeadSize - 1);
    Path q = ToPath(key, to_long, cursor, log_small_size, false);
    q.tail = p.tail;
    out->tail = 0;
    return q;
  }
  if( p.long_fp) {
    const uint64_t key = FromPathNoTail(p, from_long, log_small_size, kHeadSize);
    Path q = ToPath(key, to_long, cursor, log_small_size, false);
    q.tail = p.tail;
    out->tail = 0;
    return q;
  }
  uint64_t key = FromPathNoTail(p, from_short, log_small_size, kHeadSize - 1);
  Path q = ToPath(key, to_short, cursor, log_small_size, true);
  q.tail = p.tail;
  if (q.level >= cursor) return q;
  // q is invalid - the level is low, but there aren't enough bits for the fingerprint
  if (p.tail != 1 << kTailSize) {
    uint64_t k =
        key | ((p.tail >> kTailSize) << (64 - kLogLevels - log_small_size - kHeadSize));
    Path q2 = ToPath(k, to_long, cursor, log_small_size, true);
    q2.tail = p.tail << 1;
    out->tail = 0;
    return q2;
  }
  // p.tail is empty!
  *out = ToPath(key, to_long, cursor, log_small_size, true);
  uint64_t k = key | (1ul << (64 - kLogLevels - log_small_size - kHeadSize));
  Path q2 = ToPath(k, to_long, cursor, log_small_size, true);
  q2.tail = p.tail;
  return q2;
}

}  // namespace detail
}  // namespace filter