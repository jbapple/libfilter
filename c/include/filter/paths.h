// Usable under the terms in the Apache License, Version 2.0.
//
// The taffy filter can, unlike Cuckoo filters and Bloom filters, increase in size to
// accommodate new items as the working set grows, without major degradation to the
// storage efficiency (size * false positive probability). Roughly speaking, it is a
// cuckoo hash table that uses "quotienting" for a succinct representation.
//
// See "How to Approximate A Set Without Knowing Its Size In Advance", by
// Rasmus Pagh, Gil Segev, and Udi Wieder

#pragma once

//#include <algorithm>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
//#include <iostream>
//#include <utility>

// #include "immintrin.h"

//#include "util.hpp"

#include "filter/util.h"

#define libfilter_minimal_taffy_cuckoo_log_levels 5
#define libfilter_minimal_taffy_cuckoo_levels \
  (1ul << libfilter_minimal_taffy_cuckoo_log_levels)

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
#define libfilter_minimal_taffy_cuckoo_head_size 9
#define libfilter_minimal_taffy_cuckoo_tail_size 5

#if (libfilter_minimal_taffy_cuckoo_head_size +      \
         libfilter_minimal_taffy_cuckoo_tail_size != \
     14)
#error "kHeadSize + kTailSize == 14"
#endif

// TODO: overlapping buckets to increase occupancy

typedef struct {
  bool long_fp : 1;
  uint64_t fingerprint : libfilter_minimal_taffy_cuckoo_head_size;
  uint64_t tail : libfilter_minimal_taffy_cuckoo_tail_size +
                  1;  // +1 for the encoding of sequences above. tail == 0 indicates an
                      // empty slot, no matter what's in fingerprint
} __attribute__((packed)) libfilter_minimal_taffy_cuckoo_slot;

// INLINE void Print() const {
//   std::cout << "{" << long_fp << std::hex << ", " << fingerprint << ", " << tail <<
//   "}";
// }
// constexpr Slot() : long_fp(), fingerprint(), tail() {}

// static_assert(sizeof(Slot) == 2, "sizeof(Slot)");

// A path encodes the slot number, as well as the slot itself.
typedef struct {
  libfilter_minimal_taffy_cuckoo_slot slot;
  uint64_t level : libfilter_minimal_taffy_cuckoo_log_levels;
  uint64_t bucket;
} __attribute__((packed)) libfilter_minimal_taffy_cuckoo_path;

INLINE bool libfilter_minimal_taffy_cuckoo_path_equal(
    const libfilter_minimal_taffy_cuckoo_path here,
    const libfilter_minimal_taffy_cuckoo_path that) {
  return here.level == that.level && here.bucket == that.bucket &&
         here.slot.long_fp == that.slot.long_fp &&
         here.slot.fingerprint == that.slot.fingerprint &&
         here.slot.tail == that.slot.tail;
}

/*

A raw key can be made into up to four different paths, one or two per side. In each side
there are two hash functions for differen traw input lengths. The one for long input
length always yields a path, though what "shape" path it yields depends on the cursor and
the level. If the level is less than the cursor, then the fingerprint is short and the
index is long. Mutatis mutandis.

The one for short input yields a path iff the level is greater than or equal to the
cursor.

*/

// enum class SlotType { kLongIndex = 0, kLongFingerprint = 1, kShort = 2 };

// paths are either long or short. long paths are either before the cursor with long_fp
// false or after with long_fp true. short paths are after the cursor with long_fp false.
//
// There are no valid paths before the cursor with long_fp true.
//
// if full_is_short is true and the path is before the cursor, this function retuens an
// empty path (tail == 0).
INLINE libfilter_minimal_taffy_cuckoo_path libfilter_minimal_taffy_cuckoo_to_path(
    uint64_t raw, const libfilter_feistel* f, int cursor, uint64_t low_level_size,
    bool full_is_short) {
  const uint64_t pre_hash_level_index_fp_and_tail =
      raw >> (64 - libfilter_minimal_taffy_cuckoo_log_levels - low_level_size -
              libfilter_minimal_taffy_cuckoo_head_size + full_is_short -
              libfilter_minimal_taffy_cuckoo_tail_size);
  const uint64_t raw_tail = libfilter_mask(libfilter_minimal_taffy_cuckoo_tail_size,
                                           pre_hash_level_index_fp_and_tail);
  const uint64_t pre_hash_level_index_and_fp =
      pre_hash_level_index_fp_and_tail >> libfilter_minimal_taffy_cuckoo_tail_size;
  const uint64_t hashed_level_index_and_fp = libfilter_feistel_permute_forward(
      f,
      libfilter_minimal_taffy_cuckoo_log_levels + low_level_size +
          libfilter_minimal_taffy_cuckoo_head_size - full_is_short,
      pre_hash_level_index_and_fp);
  libfilter_minimal_taffy_cuckoo_path result;
  result.level =
      hashed_level_index_and_fp >>
      (low_level_size + libfilter_minimal_taffy_cuckoo_head_size - full_is_short);
  bool big_index = (result.level < cursor);
  if (big_index && full_is_short) {
    result.slot.tail = 0;
    return result;
  }

  result.bucket = libfilter_mask(
      low_level_size + big_index,
      hashed_level_index_and_fp >>
          (libfilter_minimal_taffy_cuckoo_head_size - full_is_short - big_index));
  result.slot.long_fp = !big_index && !full_is_short;

  result.slot.fingerprint =
      libfilter_mask(libfilter_minimal_taffy_cuckoo_head_size - full_is_short - big_index,
                     hashed_level_index_and_fp);

  // encode the tail using the encoding described above, in which the length of the tail
  // i the complement of the tzcnt plus one.
  result.slot.tail = raw_tail * 2 + 1;
  return result;
}

// Uses reverse permuting to get back the high bits of the original hashed value. Elides
// the tail, since the tail may have a limited length, and once that's appended to a raw
// value, one can't tell a short tail from one that just has a lot of zeros at the end.
INLINE uint64_t libfilter_minimal_taffy_cuckoo_from_path_no_tail(
    libfilter_minimal_taffy_cuckoo_path p, const libfilter_feistel* f,
    uint64_t level_size, uint64_t fingerprint_size) {
  const uint64_t hashed_level_index_and_fp =
      (((p.level << level_size) | p.bucket) << fingerprint_size) | p.slot.fingerprint;
  const uint64_t pre_hashed_index_and_fp = libfilter_feistel_permute_backward(
      f, libfilter_minimal_taffy_cuckoo_log_levels + level_size + fingerprint_size,
      hashed_level_index_and_fp);
  uint64_t shifted_up = pre_hashed_index_and_fp
                        << (64 - libfilter_minimal_taffy_cuckoo_log_levels - level_size -
                            fingerprint_size);
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

INLINE libfilter_minimal_taffy_cuckoo_path libfilter_minimal_taffy_cuckoo_re_path_upsize(
    libfilter_minimal_taffy_cuckoo_path p, const libfilter_feistel* flo,
    const libfilter_feistel* fhi, uint64_t log_size, int from_cursor,
    libfilter_minimal_taffy_cuckoo_path* out) {
  const int to_cursor = from_cursor + 1;
  // TODO: when log_to_size is larger than log_from_size, need to steal bits from the
  // tail?
  assert(p.slot.tail != 0);
  if (p.level < from_cursor) {
    const uint64_t key = libfilter_minimal_taffy_cuckoo_from_path_no_tail(
        p, fhi, log_size + 1, libfilter_minimal_taffy_cuckoo_head_size - 1);
    libfilter_minimal_taffy_cuckoo_path q =
        libfilter_minimal_taffy_cuckoo_to_path(key, fhi, to_cursor, log_size, false);
    q.slot.tail = p.slot.tail;
    out->slot.tail = 0;
    return q;
  }
  if (p.slot.long_fp) {
    const uint64_t key = libfilter_minimal_taffy_cuckoo_from_path_no_tail(
        p, fhi, log_size, libfilter_minimal_taffy_cuckoo_head_size);
    libfilter_minimal_taffy_cuckoo_path q =
        libfilter_minimal_taffy_cuckoo_to_path(key, fhi, to_cursor, log_size, false);
    q.slot.tail = p.slot.tail;
    out->slot.tail = 0;
    return q;
  }
  uint64_t key = libfilter_minimal_taffy_cuckoo_from_path_no_tail(
      p, flo, log_size, libfilter_minimal_taffy_cuckoo_head_size - 1);
  // TODO: what if q.tail == 0;
  libfilter_minimal_taffy_cuckoo_path q = libfilter_minimal_taffy_cuckoo_to_path(
      key, flo, to_cursor, log_size, true /* full is short because tail is short */);
  if (q.level >= to_cursor) {
    assert(q.slot.tail != 0);
    q.slot.tail = p.slot.tail;
    out->slot.tail = 0;
    return q;
  }
  // q is invalid - the level is low, but there aren't enough bits for the fingerprint
  if (p.slot.tail != 1 << libfilter_minimal_taffy_cuckoo_tail_size) {
    uint64_t k =
        key | (((uint64_t)(p.slot.tail >> libfilter_minimal_taffy_cuckoo_tail_size))
               << (64 - libfilter_minimal_taffy_cuckoo_log_levels - log_size -
                   libfilter_minimal_taffy_cuckoo_head_size));
    libfilter_minimal_taffy_cuckoo_path q2 = libfilter_minimal_taffy_cuckoo_to_path(
        k, fhi, to_cursor, log_size,
        false /* full is now long, since we added a bit to k*/);
    q2.slot.tail = p.slot.tail << 1;
    out->slot.tail = 0;
    return q2;
  }
  // p.tail is empty!
  *out = libfilter_minimal_taffy_cuckoo_to_path(key, fhi, to_cursor, log_size, false);
  out->slot.tail = p.slot.tail;
  uint64_t k = key | (1ul << (64 - libfilter_minimal_taffy_cuckoo_log_levels - log_size -
                              libfilter_minimal_taffy_cuckoo_head_size));
  libfilter_minimal_taffy_cuckoo_path q2 =
      libfilter_minimal_taffy_cuckoo_to_path(k, fhi, to_cursor, log_size, false);
  q2.slot.tail = p.slot.tail;
  return q2;
}

// Converts from one path to another. Potentially returns two paths (one in the out
// parameter) if the in path has level greater or equal to the cursor and the outpath has
// level less than the cursor.
//
// Has so many parameters to account for both converting paths between sides of a filter
// and between levels of a growing filter.
INLINE libfilter_minimal_taffy_cuckoo_path libfilter_minimal_taffy_cuckoo_re_path(
    libfilter_minimal_taffy_cuckoo_path p, const libfilter_feistel* from_short,
    const libfilter_feistel* from_long, const libfilter_feistel* to_short,
    const libfilter_feistel* to_long, uint64_t log_from_size, uint64_t log_to_size,
    int from_cursor, int to_cursor, libfilter_minimal_taffy_cuckoo_path* out) {
  // TODO: when log_to_size is larger than log_from_size, need to steal bits from the
  // tail?
  assert(p.slot.tail != 0);
  const bool upsize = log_to_size - log_from_size;
  if (p.level < from_cursor) {
    assert(!p.slot.long_fp);
    const uint64_t key = libfilter_minimal_taffy_cuckoo_from_path_no_tail(
        p, from_long, log_from_size + 1, libfilter_minimal_taffy_cuckoo_head_size - 1);
    libfilter_minimal_taffy_cuckoo_path q = libfilter_minimal_taffy_cuckoo_to_path(
        key, to_long, to_cursor, log_to_size, false /* full is not short */);
    q.slot.tail = p.slot.tail;
    out->slot.tail = 0;
    return q;
  }
  if (p.slot.long_fp) {
    const uint64_t key = libfilter_minimal_taffy_cuckoo_from_path_no_tail(
        p, from_long, log_from_size, libfilter_minimal_taffy_cuckoo_head_size);
    libfilter_minimal_taffy_cuckoo_path q = libfilter_minimal_taffy_cuckoo_to_path(
        key, upsize ? to_short : to_long, to_cursor, log_to_size, upsize);
    q.slot.tail = p.slot.tail;
    out->slot.tail = 0;
    return q;
  }
  uint64_t key = libfilter_minimal_taffy_cuckoo_from_path_no_tail(
      p, from_short, log_from_size, libfilter_minimal_taffy_cuckoo_head_size - 1);
  // TODO: what if q.tail == 0;
  libfilter_minimal_taffy_cuckoo_path q = libfilter_minimal_taffy_cuckoo_to_path(
      key, to_short, to_cursor, log_to_size,
      true /* full is short because tail is short */);
  if (!upsize && q.level >= to_cursor) {
    assert(q.slot.tail != 0);
    q.slot.tail = p.slot.tail;
    out->slot.tail = 0;
    return q;
  }
  // q is invalid - the level is low, but there aren't enough bits for the fingerprint
  if (p.slot.tail != 1 << libfilter_minimal_taffy_cuckoo_tail_size) {
    uint64_t k =
        key | (((uint64_t)(p.slot.tail >> libfilter_minimal_taffy_cuckoo_tail_size))
               << (64 - libfilter_minimal_taffy_cuckoo_log_levels - log_from_size -
                   libfilter_minimal_taffy_cuckoo_head_size));
    libfilter_minimal_taffy_cuckoo_path q2 = libfilter_minimal_taffy_cuckoo_to_path(
        k, to_long, to_cursor, log_to_size,
        false /* full is now long, since we added a bit to k*/);
    q2.slot.tail = p.slot.tail << 1;
    out->slot.tail = 0;
    return q2;
  }
  // p.tail is empty!
  *out =
      libfilter_minimal_taffy_cuckoo_to_path(key, to_long, to_cursor, log_to_size, false);
  out->slot.tail = p.slot.tail;
  uint64_t k = key | (1ul << (64 - libfilter_minimal_taffy_cuckoo_log_levels -
                              log_from_size - libfilter_minimal_taffy_cuckoo_head_size));
  libfilter_minimal_taffy_cuckoo_path q2 =
      libfilter_minimal_taffy_cuckoo_to_path(k, to_long, to_cursor, log_to_size, false);
  q2.slot.tail = p.slot.tail;
  return q2;
}
