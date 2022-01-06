// Usable under the terms in the Apache License, Version 2.0.
//
// The taffy filter can, unlike Cuckoo filters and Bloom filters, increase in size to
// accommodate new items as the working set grows, without major degradation to the
// storage efficiency (size * false positive probability). Roughly speaking, it is a
// cuckoo hash table that uses "quotienting" for a succinct representation.
//
// See "How to Approximate A Set Without Knowing Its Size In Advance", by
// Rasmus Pagh, Gil Segev, and Udi Wieder
//
// TODO: Intersection, iteration, Freeze/Thaw, serialize/deserialize

#pragma once

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "filter/util.h"

// From the paper, libfilter_taffy_cuckoo_tail_size is the log of the number of times the size will
// double, while kHead size is two more than ceil(log(1/epsilon)) + i, bit i is only used
// up  by 1 (for the sides) plus libfilter_log_slots. For instance, with libfilter_taffy_cuckoo_tail_size = 5 and
// libfilter_taffy_cuckoo_head_size = 10, log(1/epsilon) is 10 - 2 - libfilter_log_slots - 1, which is an fpp of about
// 3%. Yikes! The good news is that checking against the tails and in the early stages of
// growth, epsilon is lower. TODO: how much lower, in theory?
//
// Note that libfilter_taffy_cuckoo_head_size must be large enough for quotient cuckoo hashing to work sensibly:
// it needs some randomness in the fingerprint to ensure each item hashes to sufficiently
// different buckets kHead is just the rest of the uint16_t, and is log2(1/epsilon)
#if defined(libfilter_taffy_cuckoo_head_size)
#error "libfilter_taffy_cuckoo_head_size"
#endif

#define libfilter_taffy_cuckoo_head_size 10

#if defined (libfilter_taffy_cuckoo_tail_size)
#error "libfilter_taffy_cuckoo_tail_size"
#endif

#define libfilter_taffy_cuckoo_tail_size 5

#if libfilter_taffy_cuckoo_head_size + libfilter_taffy_cuckoo_tail_size != 15
#error "libfilter_taffy_cuckoo_head_size + libfilter_taffy_cuckoo_tail_size != 15"
#endif

// The number of slots in each cuckoo table bucket. The higher this is, the easier it is
// to do an insert but the higher the fpp.
#if defined(libfilter_log_slots)
#error "libfilter_log_slots"
#endif

#define libfilter_log_slots 2

#if defined(libfilter_slots)
#error "libfilter_slots"
#endif

#define libfilter_slots (1 << libfilter_log_slots)

typedef struct  {
  uint64_t fingerprint : libfilter_taffy_cuckoo_head_size;
  uint64_t tail : libfilter_taffy_cuckoo_tail_size +
                  1;  // +1 for the encoding of sequences above. tail == 0 indicates
                      // an empty slot, no matter what's in fingerprint
} __attribute__((packed)) Slot;

// static_assert(sizeof(Slot) == 2, "sizeof(Slot)");

// INLINE void PrintSlot(Slot s) {
//   std::cout << "{" << std::hex << s.fingerprint << ", " << s.tail << "}";
// }

// A path encodes the slot number, as well as the slot itself.
typedef struct  {
  Slot slot;
  uint64_t bucket;
} Path;

// INLINE void PrintPath(const Path* here) {
//   std::cout << "{" << std::dec << here->bucket << ", {" << std::hex << here->slot.fingerprint << ", "
//             << here->slot.tail << "}}";
// }

INLINE bool EqualPath(Path here, Path there) {
  return here.bucket == there.bucket && here.slot.fingerprint == there.slot.fingerprint &&
         here.slot.tail == there.slot.tail;
}

// Converts a hash value to a path. The bucket and the fingerprint are acquired via
// hashing, while the tail is a selection of un-hashed bits from raw. Later, when moving
// bits from the tail to the fingerprint, one must un-hash the bucket and the fingerprint,
// move the bit, then re-hash
//
// Operates on the high bits, since bits must be moved from the tail into the fingerprint
// and then bucket.
INLINE Path ToPath(uint64_t raw, const libfilter_feistel* f, uint64_t log_side_size) {
  uint64_t pre_hash_index_and_fp =
      raw >> (64 - log_side_size - libfilter_taffy_cuckoo_head_size);
  uint64_t hashed_index_and_fp = libfilter_feistel_permute_forward(
      f, log_side_size + libfilter_taffy_cuckoo_head_size, pre_hash_index_and_fp);
  uint64_t index = hashed_index_and_fp >> libfilter_taffy_cuckoo_head_size;
  Path p;
  p.bucket = index;
  // Helpfully gets cut off by being a bitfield
  p.slot.fingerprint = hashed_index_and_fp;
  uint64_t pre_hash_index_fp_and_tail =
      raw >> (64 - log_side_size - libfilter_taffy_cuckoo_head_size -
              libfilter_taffy_cuckoo_tail_size);
  uint64_t raw_tail =
      libfilter_mask(libfilter_taffy_cuckoo_tail_size, pre_hash_index_fp_and_tail);
  // encode the tail using the encoding described above, in which the length of the tail i
  // the complement of the tzcnt plus one.
  uint64_t encoded_tail = raw_tail * 2 + 1;
  p.slot.tail = encoded_tail;
  return p;
}

// Uses reverse permuting to get back the high bits of the original hashed value. Elides
// the tail, since the tail may have a limited length, and once that's appended to a raw
// value, one can't tell a short tail from one that just has a lot of zeros at the end.
INLINE uint64_t FromPathNoTail(Path p, const libfilter_feistel * f, uint64_t log_side_size) {
  uint64_t hashed_index_and_fp =
      (p.bucket << libfilter_taffy_cuckoo_head_size) | p.slot.fingerprint;
  uint64_t pre_hashed_index_and_fp = libfilter_feistel_permute_backward(
      f, log_side_size + libfilter_taffy_cuckoo_head_size, hashed_index_and_fp);
  uint64_t shifted_up = pre_hashed_index_and_fp
                        << (64 - log_side_size - libfilter_taffy_cuckoo_head_size);
  return shifted_up;
}

typedef struct {
  Slot data[libfilter_slots];
} __attribute__((packed)) Bucket;

// static_assert(sizeof(Bucket) == sizeof(Slot) * libfilter_slots, "sizeof(Bucket)");

// A cuckoo hash table is made up of sides (in some incarnations), rather than jut two
// buckets from one array. Each side has a stash that holds any paths that couldn't fit.
// This is useful for random-walk cuckoo hashing, in which the leftover path needs  place
// to be stored so it doesn't invalidate old inserts.
typedef struct  {
  libfilter_feistel f;
  Bucket* data;

  size_t stash_capacity;
  size_t stash_size;
  Path * stash;
} Side;

Side SideCreate(int log_side_size, const uint64_t* keys);

// Returns an empty path (tail = 0) if insert added a new element. Returns p if insert
// succeded without anning anything new. Returns something else if that something else
// was displaced by the insert. That item must be inserted then
INLINE Path InsertSide(Side* here, Path p, libfilter_pcg_random* rng) {
  assert(p.slot.tail != 0);
  Bucket* b = &here->data[p.bucket];
  for (int i = 0; i < libfilter_slots; ++i) {
    if (b->data[i].tail == 0) {
      // empty slot
      b->data[i] = p.slot;
      p.slot.tail = 0;
      return p;
    }
    if (b->data[i].fingerprint == p.slot.fingerprint) {
      // already present in the table
      if (libfilter_taffy_is_prefix_of(b->data[i].tail, p.slot.tail)) {
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
  int i = libfilter_pcg_random_get(rng);
  Path result = p;
  result.slot = b->data[i];
  b->data[i] = p.slot;
  return result;
}

INLINE bool Find(const Side* here, Path p) {
  assert(p.slot.tail != 0);
  for(unsigned i = 0; i < here->stash_size; ++i) {
    if (here->stash[i].slot.tail != 0 && p.bucket == here->stash[i].bucket &&
        p.slot.fingerprint == here->stash[i].slot.fingerprint &&
        libfilter_taffy_is_prefix_of(here->stash[i].slot.tail, p.slot.tail)) {
      return true;
    }
  }
  Bucket* b = &here->data[p.bucket];
  for (int i = 0; i < libfilter_slots; ++i) {
    if (b->data[i].tail == 0) continue;
    if (b->data[i].fingerprint == p.slot.fingerprint &&
        libfilter_taffy_is_prefix_of(b->data[i].tail, p.slot.tail)) {
      return true;
    }
  }
  return false;
}

/*
INLINE void swap(Side* x, Side* y) {
  using std::swap;
  swap(x->f, y->f);
  swap(x->data, y->data);
  swap(x->stash, y->stash);
  swap(x->stash_size, y->stash_size);
  swap(x->stash_capacity, y->stash_capacity);
}
*/

typedef struct {
  uint64_t zero : libfilter_taffy_cuckoo_head_size;
  uint64_t one : libfilter_taffy_cuckoo_head_size;
  uint64_t two : libfilter_taffy_cuckoo_head_size;
  uint64_t three : libfilter_taffy_cuckoo_head_size;
} __attribute__((packed)) FrozenTaffyCuckooBaseBucket;

// static_assert(sizeof(FrozenTaffyCuckooBaseBucket) ==
//                   libfilter_slots * libfilter_taffy_cuckoo_head_size / CHAR_BIT,
//               "packed");

typedef struct  {
  libfilter_feistel hash_[2];
  int log_side_size_;
  FrozenTaffyCuckooBaseBucket* data_[2];
  uint64_t* stash_[2];
  size_t stash_capacity_[2];
  size_t stash_size_[2];
} FrozenTaffyCuckooBase;

size_t FrozenSizeInBytes(const FrozenTaffyCuckooBase* b);

#define haszero10(x) (((x)-0x40100401ULL) & (~(x)) & 0x8020080200ULL)
#define hasvalue10(x, n) (haszero10((x) ^ (0x40100401ULL * (n))))

INLINE bool FrozenFindHash(const FrozenTaffyCuckooBase* here, uint64_t x) {
  for (int i = 0; i < 2; ++i) {
    uint64_t y = x >> (64 - here->log_side_size_ - libfilter_taffy_cuckoo_head_size);
    uint64_t permuted = libfilter_feistel_permute_forward(
        &here->hash_[i], here->log_side_size_ + libfilter_taffy_cuckoo_head_size, y);
    for (size_t j = 0; j < here->stash_size_[i]; ++j) {
      if (here->stash_[i][j] == permuted) return true;
    }
    FrozenTaffyCuckooBaseBucket* b = &here->data_[i][permuted >> libfilter_taffy_cuckoo_head_size];
    uint64_t fingerprint = permuted & ((1 << libfilter_taffy_cuckoo_head_size) - 1);
    if (0 == fingerprint) return true;
    // TODO: SWAR
    uint64_t z = 0;
    memcpy(&z, b, sizeof(*b));
    if (hasvalue10(z, fingerprint)) return true;
  }
  return false;
}

void FrozenTaffyCuckooBaseDestroy(FrozenTaffyCuckooBase* here);
FrozenTaffyCuckooBase FrozenTaffyCuckooBaseCreate(const uint64_t entropy[8],
                                                  int log_side_size);

typedef struct  {
  Side sides[2];
  int log_side_size;
  libfilter_pcg_random rng;
  const uint64_t* entropy;
  uint64_t occupied;
} TaffyCuckooFilterBase;

void TaffyCuckooFilterBaseSwap(TaffyCuckooFilterBase* x, TaffyCuckooFilterBase* y);
TaffyCuckooFilterBase TaffyCuckooFilterBaseCreate(int log_side_size,
                                                  const uint64_t* entropy);
TaffyCuckooFilterBase TaffyCuckooFilterBaseClone(const TaffyCuckooFilterBase* that);
TaffyCuckooFilterBase BaseCreateWithBytes(uint64_t bytes);
FrozenTaffyCuckooBase BaseFreeze(const TaffyCuckooFilterBase* here);

uint64_t TaffyCuckooSizeInBytes(const TaffyCuckooFilterBase* here);


INLINE bool BaseFindHash(const TaffyCuckooFilterBase* here, uint64_t k) {
#if defined(__clang) || defined(__clang__)
#pragma unroll
#else
#pragma GCC unroll 2
#endif
    for (int s = 0; s < 2; ++s) {
      if (Find(&here->sides[s], ToPath(k, &here->sides[s].f, here->log_side_size)))
        return true;
    }
    return false;
}

INLINE uint64_t Capacity(const TaffyCuckooFilterBase* here) {
  return 2 * libfilter_slots * (1ul << here->log_side_size);
}

// After Stashed result, HT is close to full and should be upsized
// After ttl, stash the input and return Stashed. Pre-condition: at least one stash is
// empty. Also, p is a left path, not a right one.
INLINE bool InsertTTL(TaffyCuckooFilterBase* here, int s, Path p, int ttl) {
  // if (sides[0].stash.tail != 0 && sides[1].stash.tail != 0) return
  // InsertResult::Failed;
  Side* both[2] = {&here->sides[s], &here->sides[1 - s]};
  while (true) {
#if defined(__clang) || defined(__clang__)
#pragma unroll
#else
#pragma GCC unroll 2
#endif
    for (int i = 0; i < 2; ++i) {
      Path q = p;
      p = InsertSide(both[i], p, &here->rng);
      if (p.slot.tail == 0) {
        // Found an empty slot
        ++here->occupied;
        return true;
      }
      if (EqualPath(p, q)) {
        // Combined with or already present in a slot. Success, but no increase in
        // filter size
        return true;
      }
      uint64_t tail = p.slot.tail;
      if (ttl <= 0) {
        // we've run out of room. If there's room in this stash, stash it here. If there
        // is not room in this stash, there must be room in the other, based on the
        // pre-condition for this method.
        if (both[i]->stash_size == both[i]->stash_capacity) {
          both[i]->stash_capacity *= 2;
          //std::cerr << both[i]->stash_capacity << std::endl;
          Path * new_stash = (Path*)calloc(both[i]->stash_capacity, sizeof(Path));
          memcpy(new_stash, both[i]->stash, both[i]->stash_size * sizeof(Path));
          free( both[i]->stash);
          both[i]->stash = new_stash;
        }
        both[i]->stash[both[i]->stash_size++] = p;
        ++here->occupied;
        return false;
      }
      --ttl;
      // translate p to beign a path about the right half of the table
      p = ToPath(FromPathNoTail(p, &both[i]->f, here->log_side_size),
                         &both[1 - i]->f, here->log_side_size);
      // P's tail is now artificiall 0, but it should stay the same as we move from side
      // to side
      p.slot.tail = tail;
    }
  }
}

  // This method just increases ttl until insert succeeds.
  // TODO: upsize when insert fails with high enough ttl?
INLINE bool InsertTCFB(TaffyCuckooFilterBase* here, int s, Path q) {
  int ttl = 32;
  return InsertTTL(here, s, q, ttl);
}

void TaffyCuckooFilterBaseDestroy(TaffyCuckooFilterBase* t);

// Double the size of the filter
void Upsize(TaffyCuckooFilterBase* here);

INLINE bool BaseInsertHash(TaffyCuckooFilterBase* here, uint64_t k) {
  // 95% is achievable, generally,but give it some room
  while (here->occupied > 0.90 * Capacity(here) || here->occupied + 4 >= Capacity(here) ||
         here->sides[0].stash_size + here->sides[1].stash_size > 8) {
    Upsize(here);
  }
  InsertTCFB(here, 0, ToPath(k, &here->sides[0].f, here->log_side_size));
  return true;
}

TaffyCuckooFilterBase UnionTwo(const TaffyCuckooFilterBase* x, const TaffyCuckooFilterBase* y);
