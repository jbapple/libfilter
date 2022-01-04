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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "util.hpp"

namespace filter {

namespace detail {

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
#if defined(kHeadSize)
#error "kHeadSize"
#endif

#define kHeadSize 10

#if defined (kTailSize)
#error "kTailSize"
#endif

#define kTailSize 5

#if kHeadSize + kTailSize != 15
#error "kHeadSize + kTailSize != 15"
#endif

// The number of slots in each cuckoo table bucket. The higher this is, the easier it is
// to do an insert but the higher the fpp.
#if defined(kLogBuckets)
#error "kLogBuckets"
#endif

#define kLogBuckets 2

#if defined(kBuckets)
#error "kBuckets"
#endif

#define kBuckets (1 << kLogBuckets)

struct Slot {
  uint64_t fingerprint : kHeadSize;
  uint64_t tail : kTailSize +
                  1;  // +1 for the encoding of sequences above. tail == 0 indicates
                      // an empty slot, no matter what's in fingerprint
} __attribute__((packed));

static_assert(sizeof(Slot) == 2, "sizeof(Slot)");

INLINE void PrintSlot(Slot s) {
  std::cout << "{" << std::hex << s.fingerprint << ", " << s.tail << "}";
}

// A path encodes the slot number, as well as the slot itself.
struct Path {
  Slot slot;
  uint64_t bucket;
};

INLINE void PrintPath(const Path* here) {
  std::cout << "{" << std::dec << here->bucket << ", {" << std::hex << here->slot.fingerprint << ", "
            << here->slot.tail << "}}";
}

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
INLINE Path ToPath(uint64_t raw, const Feistel* f, uint64_t log_side_size) {
  uint64_t pre_hash_index_and_fp = raw >> (64 - log_side_size - kHeadSize);
  uint64_t hashed_index_and_fp =
      f->Permute(log_side_size + kHeadSize, pre_hash_index_and_fp);
  uint64_t index = hashed_index_and_fp >> kHeadSize;
  Path p;
  p.bucket = index;
  // Helpfully gets cut off by being a bitfield
  p.slot.fingerprint = hashed_index_and_fp;
  uint64_t pre_hash_index_fp_and_tail =
      raw >> (64 - log_side_size - kHeadSize - kTailSize);
  uint64_t raw_tail = Mask(kTailSize, pre_hash_index_fp_and_tail);
  // encode the tail using the encoding described above, in which the length of the tail i
  // the complement of the tzcnt plus one.
  uint64_t encoded_tail = raw_tail * 2 + 1;
  p.slot.tail = encoded_tail;
  return p;
}

// Uses reverse permuting to get back the high bits of the original hashed value. Elides
// the tail, since the tail may have a limited length, and once that's appended to a raw
// value, one can't tell a short tail from one that just has a lot of zeros at the end.
INLINE uint64_t FromPathNoTail(Path p, const Feistel * f, uint64_t log_side_size) {
  uint64_t hashed_index_and_fp = (p.bucket << kHeadSize) | p.slot.fingerprint;
  uint64_t pre_hashed_index_and_fp =
      f->ReversePermute(log_side_size + kHeadSize, hashed_index_and_fp);
  uint64_t shifted_up = pre_hashed_index_and_fp << (64 - log_side_size - kHeadSize);
  return shifted_up;
}

struct Bucket {
  Slot data[kBuckets];
};

static_assert(sizeof(Bucket) == sizeof(Slot) * kBuckets, "sizeof(Bucket)");

// A cuckoo hash table is made up of sides (in some incarnations), rather than jut two
// buckets from one array. Each side has a stash that holds any paths that couldn't fit.
// This is useful for random-walk cuckoo hashing, in which the leftover path needs  place
// to be stored so it doesn't invalidate old inserts.
struct Side {
  Feistel f;
  Bucket* data;

  size_t stash_capacity;
  size_t stash_size;
  Path * stash;
};

Side SideCreate(int log_side_size, const uint64_t* keys) {
  Side here;
  here.f = &keys[0];
  here.data = new Bucket[1ul << log_side_size]();
  here.stash_capacity = 4;
  here.stash_size = 0;
  here.stash = new Path[here.stash_capacity]();

  return here;
}

// Returns an empty path (tail = 0) if insert added a new element. Returns p if insert
// succeded without anning anything new. Returns something else if that something else
// was displaced by the insert. That item must be inserted then
INLINE Path Insert(Side* here, Path p, PcgRandom* rng) {
  assert(p.tail != 0);
  Bucket* b = &here->data[p.bucket];
  for (int i = 0; i < kBuckets; ++i) {
    if (b->data[i].tail == 0) {
      // empty slot
      b->data[i] = p.slot;
      p.slot.tail = 0;
      return p;
    }
    if (b->data[i].fingerprint == p.slot.fingerprint) {
      // already present in the table
      if (IsPrefixOf(b->data[i].tail, p.slot.tail)) {
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
  int i = rng->Get();
  Path result = p;
  result.slot = b->data[i];
  b->data[i] = p.slot;
  return result;
}

INLINE bool Find(const Side* here, Path p) {
  assert(p.tail != 0);
  for(unsigned i = 0; i < here->stash_size; ++i) {
    if (here->stash[i].slot.tail != 0 && p.bucket == here->stash[i].bucket &&
        p.slot.fingerprint == here->stash[i].slot.fingerprint &&
        IsPrefixOf(here->stash[i].slot.tail, p.slot.tail)) {
      return true;
    }
  }
  Bucket* b = &here->data[p.bucket];
  for (int i = 0; i < kBuckets; ++i) {
    if (b->data[i].tail == 0) continue;
    if (b->data[i].fingerprint == p.slot.fingerprint && IsPrefixOf(b->data[i].tail, p.slot.tail)) {
      return true;
    }
  }
  return false;
}

INLINE void swap(Side& x, Side& y) {
  using std::swap;
  swap(x.f, y.f);
  swap(x.data, y.data);
  swap(x.stash, y.stash);
  swap(x.stash_size, y.stash_size);
  swap(x.stash_capacity, y.stash_capacity);
}

}  // namespace detail

struct FrozenTaffyCuckooBaseBucket {
  uint64_t zero : kHeadSize;
  uint64_t one : kHeadSize;
  uint64_t two : kHeadSize;
  uint64_t three : kHeadSize;
} __attribute__((packed));

static_assert(sizeof(FrozenTaffyCuckooBaseBucket) ==
                  kBuckets * kHeadSize / CHAR_BIT,
              "packed");

struct FrozenTaffyCuckooBase {
  detail::Feistel hash_[2];
  int log_side_size_;
  FrozenTaffyCuckooBaseBucket* data_[2];
  uint64_t* stash_[2];
  size_t stash_capacity_[2];
  size_t stash_size_[2];
};

size_t SizeInBytes(const FrozenTaffyCuckooBase* b) {
  return sizeof(FrozenTaffyCuckooBaseBucket) * 2ul << b->log_side_size_;
}

#define haszero10(x) (((x)-0x40100401ULL) & (~(x)) & 0x8020080200ULL)
#define hasvalue10(x, n) (haszero10((x) ^ (0x40100401ULL * (n))))

bool FindHash(const FrozenTaffyCuckooBase* here, uint64_t x) {
  for (int i = 0; i < 2; ++i) {
    uint64_t y = x >> (64 - here->log_side_size_ - kHeadSize);
    uint64_t permuted =
        here->hash_[i].Permute(here->log_side_size_ + kHeadSize, y);
    for (size_t j = 0; j < here->stash_size_[i]; ++j) {
      if (here->stash_[i][j] == permuted) return true;
    }
    FrozenTaffyCuckooBaseBucket* b = &here->data_[i][permuted >> kHeadSize];
    uint64_t fingerprint = permuted & ((1 << kHeadSize) - 1);
    if (0 == fingerprint) return true;
    // TODO: SWAR
    uint64_t z = 0;
    memcpy(&z, b, sizeof(*b));
    if (hasvalue10(z, fingerprint)) return true;
  }
  return false;
}

void FrozenTaffyCuckooBaseDestroy(FrozenTaffyCuckooBase* here) {
  delete[] here->data_[0];
  delete[] here->data_[1];
  delete[] here->stash_[0];
  delete[] here->stash_[1];
}

FrozenTaffyCuckooBase FrozenTaffyCuckooBaseCreate(const uint64_t entropy[8], int log_side_size) {
  FrozenTaffyCuckooBase here;
  here.hash_[0] = entropy;
  here.hash_[1] = &entropy[4];
  here.log_side_size_ = log_side_size;
  for (int i = 0; i < 2; ++i) {
    here.data_[i] = new FrozenTaffyCuckooBaseBucket[1ul << log_side_size]();
    here.stash_capacity_[i] = 4;
    here.stash_size_[i] = 0;
    here.stash_[i] = new uint64_t[here.stash_capacity_[i]];
  }
  return here;
}

struct TaffyCuckooFilterBase {
  detail::Side sides[2];
  uint64_t log_side_size;
  detail::PcgRandom rng;
  const uint64_t* entropy;
  uint64_t occupied;
};

TaffyCuckooFilterBase TaffyCuckooFilterBaseCreate(int log_side_size,
                                                  const uint64_t* entropy) {
  TaffyCuckooFilterBase here;
  here.sides[0] = detail::SideCreate(log_side_size, entropy);
  here.sides[1] = detail::SideCreate(log_side_size, entropy + 4);
  here.log_side_size = log_side_size;
  here.rng.bit_width = kLogBuckets;
  here.entropy = entropy;
  here.occupied = 0;
  return here;
}

TaffyCuckooFilterBase TaffyCuckooFilterBaseClone(const TaffyCuckooFilterBase* that) {
  TaffyCuckooFilterBase here;
  here.sides[0] = detail::SideCreate(that->log_side_size, that->entropy);
  here.sides[1] = detail::SideCreate(that->log_side_size, that->entropy + 4);
  here.log_side_size = that->log_side_size;
  here.rng = that->rng;
  here.entropy = that->entropy;
  here.occupied = that->occupied;
  for (int i = 0; i < 2; ++i) {
    delete[] here.sides[i].stash;
    here.sides[i].stash = new detail::Path[that->sides[i].stash_capacity]();
    here.sides[i].stash_capacity = that->sides[i].stash_capacity;
    here.sides[i].stash_size = that->sides[i].stash_size;
    memcpy(&here.sides[i].stash[0], &that->sides[i].stash[0],
           that->sides[i].stash_size * sizeof(detail::Path));
    memcpy(&here.sides[i].data[0], &that->sides[i].data[0],
           sizeof(detail::Bucket) << that->log_side_size);
  }
  return here;
}

TaffyCuckooFilterBase CreateWithBytes(uint64_t bytes) {
  thread_local constexpr const uint64_t kEntropy[8] = {
      0x2ba7538ee1234073, 0xfcc3777539b147d6, 0x6086c563576347e7, 0x52eff34ee1764465,
      0x8639cbf57f264867, 0x5a31ee34f0224ccb, 0x07a1cb8140744ee6, 0xf2296cf6a6524e9f};
  return TaffyCuckooFilterBaseCreate(
      std::max(1.0,
               log(1.0 * bytes / 2 / kBuckets / sizeof(detail::Slot)) / log(2)),
      kEntropy);
}

FrozenTaffyCuckooBase Freeze(const TaffyCuckooFilterBase* here) {
  FrozenTaffyCuckooBase result =
      FrozenTaffyCuckooBaseCreate(here->entropy, here->log_side_size);
  for (int i : {0, 1}) {
    for (size_t j = 0; j < here->sides[i].stash_size; ++j) {
      auto topush = FromPathNoTail(here->sides[i].stash[j], &here->sides[i].f,
                                   here->log_side_size);
      if (result.stash_size_[i] == result.stash_capacity_[i]) {
        result.stash_capacity_[i] *= 2;
        uint64_t* new_stash = new uint64_t[result.stash_capacity_[i]];
        memcpy(new_stash, result.stash_[i], result.stash_size_[i]);
        delete[] result.stash_[i];
        result.stash_[i] = new_stash;
      }
      result.stash_[i][result.stash_size_[i]++] = topush;
    }
    for (size_t j = 0; j < (1ul << here->log_side_size); ++j) {
      auto* out = &result.data_[i][j];
      const auto* in = &here->sides[i].data[j];
      out->zero = in->data[0].fingerprint;
      out->one = in->data[1].fingerprint;
      out->two = in->data[2].fingerprint;
      out->three = in->data[3].fingerprint;
    }
  }
  return result;
}

uint64_t SizeInBytes(const TaffyCuckooFilterBase* here) {
  return sizeof(detail::Path) *
             (here->sides[0].stash_size + here->sides[1].stash_size) +
         2 * sizeof(detail::Slot) * (1 << here->log_side_size) * kBuckets;
}

// Verifies the occupied field:
uint64_t Count(const TaffyCuckooFilterBase* here) {
  uint64_t result = 0;
  for (int s = 0; s < 2; ++s) {
    result += here->sides[s].stash_size;
    for (uint64_t i = 0; i < 1ull << here->log_side_size; ++i) {
      for (int j = 0; j < kBuckets; ++j) {
        if (here->sides[s].data[i].data[j].tail != 0) ++result;
      }
    }
  }
  return result;
}

void Print(const TaffyCuckooFilterBase* here) {
  for (int s = 0; s < 2; ++s) {
    for (size_t i = 0; i < here->sides[s].stash_size; ++i) {
      PrintPath(&here->sides[s].stash[i]);
      std::cout << std::endl;
    }
    for (uint64_t i = 0; i < 1ull << here->log_side_size; ++i) {
      for (int j = 0; j < kBuckets; ++j) {
        PrintSlot(here->sides[s].data[i].data[j]);
        std::cout << std::endl;
      }
    }
  }
}

INLINE bool FindHash(const TaffyCuckooFilterBase* here, uint64_t k) {
#if defined(__clang) || defined(__clang__)
#pragma unroll
#else
#pragma GCC unroll 2
#endif
    for (int s = 0; s < 2; ++s) {
      if (Find(&here->sides[s], detail::ToPath(k, &here->sides[s].f, here->log_side_size)))
        return true;
    }
    return false;
}

INLINE uint64_t Capacity(const TaffyCuckooFilterBase* here) {
  return 2 * kBuckets * (1ul << here->log_side_size);
}

INLINE bool Insert(TaffyCuckooFilterBase* here, int s, detail::Path q);
void Upsize(TaffyCuckooFilterBase* here);

INLINE bool InsertHash(TaffyCuckooFilterBase* here, uint64_t k) {
  // 95% is achievable, generally,but give it some room
  while (here->occupied > 0.90 * Capacity(here) || here->occupied + 4 >= Capacity(here) ||
         here->sides[0].stash_size + here->sides[1].stash_size > 8) {
    Upsize(here);
  }
  Insert(here, 0, detail::ToPath(k, &here->sides[0].f, here->log_side_size));
  return true;
}

// After Stashed result, HT is close to full and should be upsized
// After ttl, stash the input and return Stashed. Pre-condition: at least one stash is
// empty. Also, p is a left path, not a right one.
INLINE bool InsertTTL(TaffyCuckooFilterBase* here, int s, detail::Path p, int ttl) {
  // if (sides[0].stash.tail != 0 && sides[1].stash.tail != 0) return
  // InsertResult::Failed;
  detail::Side* both[2] = {&here->sides[s], &here->sides[1 - s]};
  while (true) {
#if defined(__clang) || defined(__clang__)
#pragma unroll
#else
#pragma GCC unroll 2
#endif
    for (int i = 0; i < 2; ++i) {
      detail::Path q = p;
      p = Insert(both[i], p, &here->rng);
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
      auto tail = p.slot.tail;
      if (ttl <= 0) {
        // we've run out of room. If there's room in this stash, stash it here. If there
        // is not room in this stash, there must be room in the other, based on the
        // pre-condition for this method.
        if (both[i]->stash_size == both[i]->stash_capacity) {
          both[i]->stash_capacity *= 2;
          //std::cerr << both[i]->stash_capacity << std::endl;
          detail::Path * new_stash = new detail::Path[both[i]->stash_capacity]();
          memcpy(new_stash, both[i]->stash, both[i]->stash_size * sizeof(detail::Path));
          delete[] both[i]->stash;
          both[i]->stash = new_stash;
        }
        both[i]->stash[both[i]->stash_size++] = p;
        ++here->occupied;
        return false;
      }
      --ttl;
      // translate p to beign a path about the right half of the table
      p = detail::ToPath(detail::FromPathNoTail(p, &both[i]->f, here->log_side_size),
                         &both[1 - i]->f, here->log_side_size);
      // P's tail is now artificiall 0, but it should stay the same as we move from side
      // to side
      p.slot.tail = tail;
    }
  }
}

  // This method just increases ttl until insert succeeds.
  // TODO: upsize when insert fails with high enough ttl?
INLINE bool Insert(TaffyCuckooFilterBase* here, int s, detail::Path q) {
  int ttl = 32;
  return InsertTTL(here, s, q, ttl);
}

// Take an item from slot sl with bucket index i, a filter u that sl is in, a side that
// sl is in, and a filter to move sl to, does so, potentially inserting TWO items in t,
// as described in the paper.
INLINE void UpsizeHelper(TaffyCuckooFilterBase* here, detail::Slot sl, uint64_t i, int s,
                         TaffyCuckooFilterBase* t) {
  if (sl.tail == 0) return;
  detail::Path p;
  p.slot = sl;
  p.bucket = i;
  uint64_t q = detail::FromPathNoTail(p, &here->sides[s].f, here->log_side_size);
  if (sl.tail == 1ul << kTailSize) {
    // There are no tail bits left! Insert two values.
    // First, hash to the left side of the larger table.
    p = detail::ToPath(q, &t->sides[0].f, t->log_side_size);
    // Still no tail! :-)
    p.slot.tail = sl.tail;
    Insert(t, 0, p);
    // change the raw value by just one bit: its last
    q |= (1ul << (64 - here->log_side_size - kHeadSize - 1));
    p = detail::ToPath(q, &t->sides[0].f, t->log_side_size);
    p.slot.tail = sl.tail;
    Insert(t, 0, p);
  } else {
    // steal a bit from the tail
    q |= static_cast<uint64_t>(sl.tail >> kTailSize)
         << (64 - here->log_side_size - kHeadSize - 1);
    detail::Path r = detail::ToPath(q, &t->sides[0].f, t->log_side_size);
    r.slot.tail = (sl.tail << 1);
    Insert(t, 0, r);
  }
}

// Double the size of the filter
void Upsize(TaffyCuckooFilterBase* here) {
  TaffyCuckooFilterBase t =
      TaffyCuckooFilterBaseCreate(1 + here->log_side_size, here->entropy);

  for (int s : {0, 1}) {
    for (size_t i = 0; i < here->sides[s].stash_size; ++i) {
      UpsizeHelper(here, here->sides[s].stash[i].slot, here->sides[s].stash[i].bucket, s,
                   &t);
    }
    for (unsigned i = 0; i < (1u << here->log_side_size); ++i) {
      for (int j = 0; j < kBuckets; ++j) {
        detail::Slot sl = here->sides[s].data[i].data[j];
        UpsizeHelper(here, sl, i, s, &t);
      }
    }
  }
  using std::swap;
  swap(*here, t);
  delete[] t.sides[0].data;
  delete[] t.sides[0].stash;
  delete[] t.sides[1].data;
  delete[] t.sides[1].stash;
}

void UnionHelp(TaffyCuckooFilterBase* here, const TaffyCuckooFilterBase* that, int side,
               detail::Path p) {
  uint64_t hashed = detail::FromPathNoTail(p, &that->sides[side].f, that->log_side_size);
  // hashed is high that->log_side_size + kHeadSize, in high bits of 64-bit word
  int tail_size = kTailSize - __builtin_ctz(p.slot.tail);
  if (that->log_side_size == here->log_side_size) {
    detail::Path q = detail::ToPath(hashed, &here->sides[0].f, here->log_side_size);
    q.slot.tail = p.slot.tail;
    Insert(here, 0, q);
    q.slot.tail = 0;  // dummy line just to break on
  } else if (that->log_side_size + tail_size >= here->log_side_size) {
    uint64_t orin3 =
        (static_cast<uint64_t>(p.slot.tail & (p.slot.tail - 1))
         << (64 - that->log_side_size - kHeadSize - kTailSize - 1));
    assert((hashed & orin3) == 0);
    hashed |= orin3;
    detail::Path q = ToPath(hashed, &here->sides[0].f, here->log_side_size);
    q.slot.tail = (p.slot.tail << (here->log_side_size - that->log_side_size));
    Insert(here, 0, q);
  } else {
    // p.tail & (p.tail - 1) removes the final 1 marker. The resulting length is
    // 0, 1, 2, 3, 4, or 5. It is also tail_size, but is packed in high bits of a
    // section with size kTailSize + 1.
    uint64_t orin2 =
        (static_cast<uint64_t>(p.slot.tail & (p.slot.tail - 1))
         << (64 - that->log_side_size - kHeadSize - kTailSize - 1));
    assert(0 == (orin2 & hashed));
    hashed |= orin2;
    // The total size is now that->log_side_size + kHeadSize + tail_size
    //
    // To fill up the required log_size_size + kHeadSize, we need values of width up to
    // log_size_size + kHeadSize - (that->log_side_size + kHeadSize +
    // tail_size)
    for (uint64_t i = 0;
         i < (1u << (here->log_side_size - that->log_side_size - tail_size)); ++i) {
      // To append these, need to shift up to that->log_side_size + kHeadSize +
      // tail_size
      uint64_t orin = (i << (64 - here->log_side_size - kHeadSize));
      assert(0 == (orin & hashed));
      uint64_t tmphashed = (hashed | orin);
      detail::Path q = ToPath(tmphashed, &here->sides[0].f, here->log_side_size);
      q.slot.tail = (1u << kTailSize);
      Insert(here, 0, q);
    }
  }
}

void UnionOne(TaffyCuckooFilterBase* here, const TaffyCuckooFilterBase& that) {
  assert(that.log_side_size <= log_side_size);
  detail::Path p;
  for (int side : {0, 1}) {
    for (size_t i = 0; i < that.sides[side].stash_size; ++i) {
      UnionHelp(here, &that, side, that.sides[side].stash[i]);
    }
    for (uint64_t bucket = 0; bucket < (1ul << that.log_side_size); ++bucket) {
      p.bucket = bucket;
      for (int slot = 0; slot < kBuckets; ++slot) {
        if (that.sides[side].data[bucket].data[slot].tail == 0) continue;
        p.slot.fingerprint = that.sides[side].data[bucket].data[slot].fingerprint;
        p.slot.tail = that.sides[side].data[bucket].data[slot].tail;
        UnionHelp(here, &that, side, p);
        continue;
      }
    }
  }
}

TaffyCuckooFilterBase UnionTwo(const TaffyCuckooFilterBase& x, const TaffyCuckooFilterBase& y) {
  if (x.occupied > y.occupied) {
    TaffyCuckooFilterBase result = TaffyCuckooFilterBaseClone(&x);
    UnionOne(&result, y);
    return result;
  }
  TaffyCuckooFilterBase result = TaffyCuckooFilterBaseClone(&y);
  UnionOne(&result, x);
  return result;
}

INLINE void swap(TaffyCuckooFilterBase& x, TaffyCuckooFilterBase& y) {
  using std::swap;
  swap(x.sides[0], y.sides[0]);
  swap(x.sides[1], y.sides[1]);
  swap(x.log_side_size, y.log_side_size);
  swap(x.rng, y.rng);
  swap(x.entropy, y.entropy);
  swap(x.occupied, y.occupied);
}

////////////////////////////////////////////////////////////////////////////////

struct FrozenTaffyCuckoo {
  FrozenTaffyCuckooBase b;
  bool FindHash(uint64_t x) const { return filter::FindHash(&b, x); }

  size_t SizeInBytes() const { return filter::SizeInBytes(&b); }
  // bool InsertHash(uint64_t hash);

  INLINE static const char* Name() {
    thread_local const constexpr char result[] = "FrozenTaffyCuckoo";
    return result;
  }

  ~FrozenTaffyCuckoo() { FrozenTaffyCuckooBaseDestroy(&b); }
};

struct TaffyCuckooFilter {
  TaffyCuckooFilterBase b;

  // TODO: change to int64_t and prevent negatives
  static TaffyCuckooFilter CreateWithBytes(size_t bytes) {
    return TaffyCuckooFilter{filter::CreateWithBytes(bytes)};
  }

  static const char* Name() {
    thread_local const constexpr char result[] = "TaffyCuckoo";
    return result;
  }

  bool InsertHash(uint64_t h) { return filter::InsertHash(&b, h); }
  bool FindHash(uint64_t h) const { return filter::FindHash(&b, h); }
  size_t SizeInBytes() const { return filter::SizeInBytes(&b); }
  FrozenTaffyCuckoo Freeze() const { return {filter::Freeze(&b)}; }
  ~TaffyCuckooFilter() {
    delete[] b.sides[0].data;
    delete[] b.sides[0].stash;
    delete[] b.sides[1].data;
    delete[] b.sides[1].stash;
  }
};

TaffyCuckooFilter Union(const TaffyCuckooFilter& x, const TaffyCuckooFilter& y) {
  return {UnionTwo(x.b, y.b)};
}

}  // namespace filter

#undef kHeadSize
#undef kTailSize
#undef kLogBuckets
#undef kBuckets
