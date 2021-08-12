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

// #include "immintrin.h"

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
thread_local const constexpr int kHeadSize = 10;
thread_local const constexpr int kTailSize = 5;
static_assert(kHeadSize + kTailSize == 15, "kHeadSize + kTailSize == 15");

// The number of slots in each cuckoo table bucket. The higher this is, the easier it is
// to do an insert but the higher the fpp.
thread_local const constexpr int kLogBuckets = 2;
thread_local const constexpr int kBuckets = 1 << kLogBuckets;

struct Slot {
  uint64_t fingerprint : kHeadSize;
  uint64_t tail : kTailSize +
                  1;  // +1 for the encoding of sequences above. tail == 0 indicates an
                      // empty slot, no matter what's in fingerprint
  INLINE void Print() const {
    std::cout << "{" << std::hex << fingerprint << ", " << tail << "}";
  }
} __attribute__((packed));

static_assert(sizeof(Slot) == 2, "sizeof(Slot)");

// A path encodes the slot number, as well as the slot itself.
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

// Converts a hash value to a path. The bucket and the fingerprint are acquired via
// hashing, while the tail is a selection of un-hashed bits from raw. Later, when moving
// bits from the tail to the fingerprint, one must un-hash the bucket and the fingerprint,
// move the bit, then re-hash
//
// Operates on the high bits, since bits must be moved from the tail into the fingerprint
// and then bucket.
INLINE Path ToPath(uint64_t raw, const Feistel& f, uint64_t log_side_size) {
  uint64_t pre_hash_index_and_fp = raw >> (64 - log_side_size - kHeadSize);
  uint64_t hashed_index_and_fp =
      f.Permute(log_side_size + kHeadSize, pre_hash_index_and_fp);
  uint64_t index = hashed_index_and_fp >> kHeadSize;
  Path p;
  p.bucket = index;
  // Helpfully gets cut off by being a bitfield
  p.fingerprint = hashed_index_and_fp;
  uint64_t pre_hash_index_fp_and_tail =
      raw >> (64 - log_side_size - kHeadSize - kTailSize);
  uint64_t raw_tail = Mask(kTailSize, pre_hash_index_fp_and_tail);
  // encode the tail using the encoding described above, in which the length of the tail i
  // the complement of the tzcnt plus one.
  uint64_t encoded_tail = raw_tail * 2 + 1;
  p.tail = encoded_tail;
  return p;
}

// Uses reverse permuting to get back the high bits of the original hashed value. Elides
// the tail, since the tail may have a limited length, and once that's appended to a raw
// value, one can't tell a short tail from one that just has a lot of zeros at the end.
INLINE uint64_t FromPathNoTail(Path p, const Feistel& f, uint64_t log_side_size) {
  uint64_t hashed_index_and_fp = (p.bucket << kHeadSize) | p.fingerprint;
  uint64_t pre_hashed_index_and_fp =
      f.ReversePermute(log_side_size + kHeadSize, hashed_index_and_fp);
  uint64_t shifted_up = pre_hashed_index_and_fp << (64 - log_side_size - kHeadSize);
  return shifted_up;
}

struct Bucket {
  Slot data[kBuckets] = {};
  INLINE Slot& operator[](uint64_t x) { return data[x]; }
  INLINE const Slot& operator[](uint64_t x) const { return data[x]; }
};

static_assert(sizeof(Bucket) == sizeof(Slot) * kBuckets, "sizeof(Bucket)");

// A cuckoo hash table is made up of sides (in some incarnations), rather than jut two
// buckets from one array. Each side has a stash that holds any paths that couldn't fit.
// This is useful for random-walk cuckoo hashing, in which the leftover path needs  place
// to be stored so it doesn't invalidate old inserts.
struct Side {
  Feistel f;
  Bucket* data;
  std::vector<Path> stash;

  INLINE Side(int log_side_size, const uint64_t* keys)
      : f(&keys[0]), data(new Bucket[1ul << log_side_size]()), stash() {}

  INLINE ~Side() { delete[] data; }

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
      if (b[i].fingerprint == p.fingerprint) {
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
    for (auto& path : stash) {
      if (path.tail != 0 && p.bucket == path.bucket &&
          p.fingerprint == path.fingerprint && IsPrefixOf(path.tail, p.tail)) {
        // std::cout << "match\n";
        return true;
      }
    }
    Bucket& b = data[p.bucket];
    for (int i = 0; i < kBuckets; ++i) {
      if (b[i].tail == 0) continue;
      if (b[i].fingerprint == p.fingerprint && IsPrefixOf(b[i].tail, p.tail)) {
        //std::cout << "match\n";
        return true;
      }
    }
    return false;
  }

  friend void swap(Side&, Side&);
};

 INLINE void swap(Side& x, Side& y) {
  using std::swap;
  swap(x.f, y.f);
  auto* tmp = x.data;
  x.data = y.data;
  y.data = tmp;
  swap(x.stash, y.stash);
}

}  // namespace detail

struct TaffyCuckooFilter {
  INLINE static const char* Name() {
    thread_local const constexpr char result[] = "TaffyCuckoo";
    return result;
  }

 protected:
  detail::Side sides[2];
  uint64_t log_side_size;
  detail::PcgRandom rng = {detail::kLogBuckets};
  const uint64_t* entropy;

 public:
  uint64_t occupied = 0;

  TaffyCuckooFilter(TaffyCuckooFilter&&);
  TaffyCuckooFilter(const TaffyCuckooFilter& that)
      : sides{{(int)that.log_side_size, that.entropy},
              {(int)that.log_side_size, that.entropy + 4}},
        log_side_size(that.log_side_size),
        rng(that.rng),
        entropy(that.entropy),
        occupied(that.occupied) {
    for (int i = 0; i < 2; ++i) {
      memcpy(sides[i].data, that.sides[i].data,
             sizeof(detail::Bucket) << that.log_side_size);
    }
  }
  TaffyCuckooFilter& operator=(const TaffyCuckooFilter& that) {
    this->~TaffyCuckooFilter();
    new (this) TaffyCuckooFilter(that);
    return *this;
  }

  ~TaffyCuckooFilter() {
    //std::cout << occupied << " " << Capacity() << " " << SizeInBytes() << std::endl;
  }

  uint64_t SizeInBytes() const {
    return sizeof(detail::Path) * (sides[0].stash.size() + sides[1].stash.size()) +
           2 * sizeof(detail::Slot) * (1 << log_side_size) * detail::kBuckets;
  }

 protected:
  // Verifies the occupied field:
  uint64_t Count() const {
    uint64_t result = 0;
    for (int s = 0; s < 2; ++s) {
      result += sides[s].stash.size();
      for (uint64_t i = 0; i < 1ull << log_side_size; ++i) {
        for (int j = 0; j < detail::kBuckets; ++j) {
          if (sides[s][i][j].tail != 0) ++result;
        }
      }
    }
    return result;
  }

 public:
  void Print() const {
    for (int s = 0; s < 2; ++s) {
      for (auto& p : sides[s].stash) {
        p.Print();
        std::cout << std::endl;
      }
      for (uint64_t i = 0; i < 1ull << log_side_size; ++i) {
        for (int j = 0; j < detail::kBuckets; ++j) {
          sides[s][i][j].Print();
          std::cout << std::endl;
        }
      }
    }
  }

  TaffyCuckooFilter(int log_side_size, const uint64_t* entropy)
      : sides{{log_side_size, entropy}, {log_side_size, entropy + 6}},
        log_side_size(log_side_size),
        entropy(entropy) {}

  static TaffyCuckooFilter CreateWithBytes(uint64_t bytes) {
    thread_local constexpr const uint64_t kEntropy[13] = {
        0x2ba7538ee1234073, 0xfcc3777539b147d6, 0x6086c563576347e7, 0x52eff34ee1764465,
        0x8639cbf57f264867, 0x5a31ee34f0224ccb, 0x07a1cb8140744ee6, 0xf2296cf6a6524e9f,
        0x28a31cec9f6d4484, 0x688f3fe9de7245f6, 0x1dc17831966b41a2, 0xf227166e425e4b0c,
        0x15ab11b1a6bf4ea8};
    return TaffyCuckooFilter(
        std::max(1.0,
                 log(1.0 * bytes / 2 / detail::kBuckets / sizeof(detail::Slot)) / log(2)),
        kEntropy);
   }

  INLINE bool FindHash(uint64_t k) const {
#if defined(__clang)
#pragma unroll
#else
#pragma GCC unroll 2
#endif
    for (int s = 0; s < 2; ++s) {
      if (sides[s].Find(detail::ToPath(k, sides[s].f, log_side_size))) return true;
    }
    return false;
  }

  INLINE uint64_t Capacity() const { return  2 * detail::kBuckets * (1ul << log_side_size); }

  INLINE bool InsertHash(uint64_t k) {
    // 95% is achievable, generally,but give it some room
    while (occupied > 0.90 * Capacity() || occupied + 4 >= Capacity() || sides[0].stash.size() + sides[1].stash.size() > 8) {
      Upsize();
      //std::cout << occupied << " " << Capacity() << " " << SizeInBytes() << std::endl;
    }
    Insert(0, detail::ToPath(k, sides[0].f, log_side_size));
    return true;
  }

 protected:
  // After Stashed result, HT is close to full and should be upsized
  enum class InsertResult { Ok, Stashed, Failed };

  // After ttl, stash the input and return Stashed. Pre-condition: at least one stash is
  // empty. Also, p is a left path, not a right one.
  INLINE InsertResult Insert(int s, detail::Path p, int ttl) {
    //if (sides[0].stash.tail != 0 && sides[1].stash.tail != 0) return InsertResult::Failed;
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
        if (ttl <= 0) {
          // we've run out of room. If there's room in this stash, stash it here. If there
          // is not room in this stash, there must be room in the other, based on the
          // pre-condition for this method.
          both[i]->stash.push_back(p);
          ++occupied;
          return InsertResult::Stashed;
        }
        --ttl;
        // translate p to beign a path about the right half of the table
        p = detail::ToPath(detail::FromPathNoTail(p, both[i]->f, log_side_size),
                           both[1 - i]->f, log_side_size);
        // P's tail is now artificiall 0, but it should stay the same as we move from side
        // to side
        p.tail = tail;
      }
    }
  }

  // This method just increases ttl until insert succeeds.
  // TODO: upsize when insert fails with high enough ttl?
  INLINE InsertResult Insert(int s, detail::Path q) {
    int ttl = 32;
    //If one stash is empty, we're fine. Only go here if both stashes are full
    // while (sides[0].stash.tail != 0 && sides[1].stash.tail != 0) {
    //   ttl = 2 * ttl;
// #if defined(__clang)
// #pragma unroll
// #else
// #pragma GCC unroll 2
// #endif
//       for (int i = 0; i < 2; ++i) {
//         int t = s^i;
//         // remove stash value
//         for (auto p : sides[t].stash) {
//         sides[t].stash.tail = 0;
//         --occupied;
//         // Insert it. We know this will succeed, as one stash is now empty, but we hope it
//         // succeeds with Ok, not Stashed.
//         InsertResult result = Insert(t, p, ttl);
//         assert(result != InsertResult::Failed);
//         // At least one stash is now free. Can now do the REAL insert (after the loop)
//         if (result == InsertResult::Ok) break;
//       }
    // }
    // Can totally punt here. IOW, the stashes are ALWAYS full, since we don't even try
    // very hard not to fill them!
    // while (occupied > 0.9 * Capacity() || occupied + 4 > Capacity() ||
    //        sides[0].stash.size() + sides[1].stash.size() > 32) {
    //   Upsize();
    // }
    return Insert(s, q, ttl);
  }
/*
  void Union(const TaffyCuckooFilter& x) {
    assert(x.log_side_size <= log_side_size);
    for (int s : {0, 1}) {
      for (uint64_t b = 0; b < (1ul << x.log_side_size); ++b) {
        for (int i = 0; i < kBuckets; ++i) {
          if (x.sides[s][b][i].tail == 0) continue;
          detail::Path p;
          p.bucket = b;
          b.tail = x.sides[s][b][i].tail;
          b.fingerprint = x.sides[s][b][i].fingerprint;
          uint64_t hash = FromPathNoTail(p, x.sides[s].f, x.log_side_size);
          int (w = x.log_side_size; w < log_side_size; ++w) {
            if () {
            }
          }
        }
      }
    }
  }
*/
  friend void swap(TaffyCuckooFilter&, TaffyCuckooFilter&);

  // Take an item from slot sl with bucket index i, a filter u that sl is in, a side that
  // sl is in, and a filter to move sl to, does so, potentially inserting TWO items in t,
  // as described in the paper.
  INLINE void UpsizeHelper(detail::Slot sl, uint64_t i, TaffyCuckooFilter* u, int s,
                           TaffyCuckooFilter& t) {
    if (sl.tail == 0) return;
    detail::Path p;
    static_cast<detail::Slot&>(p) = sl;
    p.bucket = i;
    uint64_t q = detail::FromPathNoTail(p, u->sides[s].f, u->log_side_size);
    if (sl.tail == 1ul << detail::kTailSize) {
      // There are no tail bits left! Insert two values.
      // First, hash to the left side of the larger table.
      p = detail::ToPath(q, t.sides[0].f, t.log_side_size);
      // Still no tail! :-)
      p.tail = sl.tail;
      t.Insert(0, p);
      // change theraw value by just one bit: its last
      q |= (1ul << (64 - u->log_side_size - detail::kHeadSize - 1));
      p = detail::ToPath(q, t.sides[0].f, t.log_side_size);
      p.tail = sl.tail;
      t.Insert(0, p);
    } else {
      // steal a bit from the tail
      q |= static_cast<uint64_t>(sl.tail >> detail::kTailSize)
           << (64 - u->log_side_size - detail::kHeadSize - 1);
      detail::Path r = detail::ToPath(q, t.sides[0].f, t.log_side_size);
      r.tail = (sl.tail << 1);
      t.Insert(0, r);
    }
  }

  // Double the size of the filter
  void Upsize() {
    //std::cout << Capacity() << std::endl;
    TaffyCuckooFilter t(1 + log_side_size, entropy);

    std::vector<detail::Path> stashes[2] = {std::vector<detail::Path>(),
                                            std::vector<detail::Path>()};
    using std::swap;
    swap(stashes[0], sides[0].stash);
    swap(stashes[1], sides[1].stash);
    occupied = occupied - stashes[0].size();
    occupied = occupied - stashes[1].size();
    sides[0].stash.clear();
    sides[1].stash.clear();
    for (int s : {0, 1}) {
      // std::cout << (s == 0 ? "left" : "right") << std::endl;
      for (auto stash : stashes[s]) {
        // std::cout << "stash" << std::endl;
        //Unstash(500);
        UpsizeHelper(stash, stash.bucket, this, s, t);
      }
      for (unsigned i = 0; i < (1u << log_side_size); ++i) {
        for (int j = 0; j < detail::kBuckets; ++j) {
          detail::Slot sl = sides[s][i][j];
          UpsizeHelper(sl, i, this, s, t);
        }
      }
    }
    using std::swap;
    swap(*this, t);
  }
};

INLINE void swap(TaffyCuckooFilter& x, TaffyCuckooFilter& y) {
  using std::swap;
  swap(x.sides[0], y.sides[0]);
  swap(x.sides[1], y.sides[1]);
  swap(x.log_side_size, y.log_side_size);
  swap(x.rng, y.rng);
  swap(x.entropy, y.entropy);
  swap(x.occupied, y.occupied);
}

}  // namespace filter
