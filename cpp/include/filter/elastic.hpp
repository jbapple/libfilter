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

  // Returns the lowest k bits of x
INLINE uint64_t Mask(int w, uint64_t x) { return x & ((1ul << w) - 1); }

// Feistel is a permutation that is also a hash function, based on a Feistel permutation.
struct Feistel {
  // The salt for the hash functions. The component hash function is strong
  // multiply-shift. TODO: really only need four salts here, not 6.
  uint64_t keys[3][2];

  INLINE uint64_t Lo(int, int, int w, uint64_t x) const { return Mask(w, x); }
  // Returns the high bits of x. if w is s, returns the t high bits. If W is t, returns
  // the s high bits
  INLINE uint64_t Hi(int s, int t, int w, uint64_t x) const {
    return Mask(w, x >> (s + t - w));
  }

  INLINE Feistel(const void* entropy) { memcpy(keys, entropy, 3 * 2 * sizeof(uint64_t)); }

  // Applies strong multiply-shift to the w low bits of x, returning the high w bits
  INLINE uint64_t ApplyOnce(int s, int t, int w, uint64_t x, const uint64_t k[2]) const {
    return Hi(s, t, s + t - w, Mask(w, x) * Mask(s + t, k[0]) + Mask(s + t, k[1]));
  }

  // Performs the hash function "forwards". w is the width of x. This is ASYMMETRIC Feistel.
  INLINE uint64_t Permute(int w, uint64_t x) const {
    // s is floor(w/2), t is ceil(w/2).
    auto s = w >> 1;
    auto t = w - s;

    // break up x into the low s bits and the high t bits
    auto l0 = Lo(s, t, s, x);
    auto r0 = Hi(s, t, t, x);

    // First feistel application: switch the halves. l1 has t bits, while r1 has s bits,
    // xored with the t-bit hash of r0.
    auto l1 = r0;                                    // t
    auto r1 = l0 ^ ApplyOnce(s, t, t, r0, keys[0]);  // s

    // l2 has t bits. r2 has t-bits, xored with the s-bit hash of r1, which really has t
    // bits. This is permitted because strong-multiply shift is still strong if you mask
    // it.
    auto l2 = r1;                                    // s
    auto r2 = l1 ^ ApplyOnce(s, t, s, r1, keys[1]);  // t

    // The validity of this is really only seen when understanding the reverse permutation
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

  friend void swap(Feistel&, Feistel&);
};

INLINE void swap(Feistel& x, Feistel& y) {
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 2; ++j) {
      using std::swap;
      swap(x.keys[i][j], y.keys[i][j]);
    }
  }
}

struct PcgRandom {
  // *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
  // Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

  int bit_width;  // The only construction parameter - how many bits to slice off the rng
                  // each time. IOW, this will give you up to 32 bits at a time. How many
                  // do you need? We can save RNG calls by caching the remainder
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
};

// Given non-zero x and y, consider eash as a sequence of up to 15 [sic] bits. The length
// of the sequence is encoded in the lower order bits. specifically, if the lowest n bits
// are zero and the next higher bit is one, then the sequence encoded has length15 - n. It
// is stored in the high-order bits of the uint16_t. Examples, using 8 bits:
//
// 01010101 is the sequence 0101010
// 11001100 is the sequence 11001
// 00000000 is invalid
// 10000000 is the empty sequence
//
// Given all that, consider non-zero x and y as valid sequences. IsPrefixOf returns true
// if an only if the sequence represented by x can be extended to the sequence represented
// by y.
#if not defined(__LZCNT__)
constexpr
#endif
INLINE bool IsPrefixOf(uint16_t x, uint16_t y) {
  // assert(x != 0);
  // assert(y != 0);
  auto a = x ^ y;
  auto c = __builtin_ctz(x);
  auto h = __builtin_ctz(y);
#if defined(__LZCNT__)
  int i = _lzcnt_u32(a);
#else
  auto i = (a == 0) ? 32 : __builtin_clz(a);
#endif
  return (c >= h) && (i >= 31 - c);
}

// These static asserts are forbidden by the use of _lzcnt_u32

#if not defined(__LZCNT__)
static_assert(IsPrefixOf(2, 1), "IsPrefixOf(2, 1)");
static_assert(IsPrefixOf(2, 3), "IsPrefixOf(2, 3)");
static_assert(IsPrefixOf(4, 1), "IsPrefixOf(4, 1)");

static_assert(not IsPrefixOf(1, 3), "IsPrefixOf(1, 3)");
static_assert(not IsPrefixOf(1, 2), "IsPrefixOf(1, 2)");

static_assert(not IsPrefixOf(3, 1), "IsPrefixOf(3, 1)");
static_assert(not IsPrefixOf(3, 2), "IsPrefixOf(3, 2)");
static_assert(not IsPrefixOf(5, 2), "IsPrefixOf(5, 2)");
static_assert(not IsPrefixOf(6, 2), "IsPrefixOf(6, 2)");
static_assert(not IsPrefixOf(7, 2), "IsPrefixOf(7, 2)");

static_assert(not IsPrefixOf(2, 5), "IsPrefixOf(2, 5)");
static_assert(not IsPrefixOf(2, 6), "IsPrefixOf(2, 6)");
static_assert(not IsPrefixOf(2, 7), "IsPrefixOf(2, 7)");

static_assert(IsPrefixOf(16384, 1), "IsPrefixOf(16384, 1)");
#endif

// Return the combined value if x and y are the same except for their last
// digit. Can be used during insert to elide some inserts by setting their prefix as the
// value in the table slot, but rarely combinable, so doesn't seem to make much of a
// difference. The reason is that most of the keys have long tails, and long tails only
// match some small percent of the time.
//
// Returns 0 if no match
INLINE constexpr uint16_t Combinable(uint16_t x, uint16_t y) {
  // assert x != y, x != 0, y != 0, x >> 15 == 0, y >> 15 == 0
  uint32_t xy = x ^ y;
  uint32_t low = __builtin_ctz(xy);
  uint32_t high = __builtin_clz(xy);
  uint32_t xlow = __builtin_ctz(x);
  uint32_t ylow = __builtin_ctz(y);
  if (low + high == 31 && xlow == ylow && xlow  + 1 == low) {
    // This depends on the width of x and y to be 15 bits or less.
    return (x + y) / 2;
  }
  return 0;
}

static_assert(Combinable(1, 3) == 2, "Combinable(1, 3)");
static_assert(Combinable(5, 7) == 6, "Combinable(5, 7)");
static_assert(Combinable(2, 6) == 4, "Combinable(2, 6)");
static_assert(Combinable(1, 5) == 0, "Combinable(1, 5)");
static_assert(Combinable(1, 6) == 0, "Combinable(1, 6)");

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
  Path stash;

  INLINE Side(int log_side_size, const uint64_t* keys)
      : f(&keys[0]), data(new Bucket[1ul << log_side_size]()) {
    static_cast<Slot&>(stash) = {0, 0};
    stash.tail = 0;
  }
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
    if (p.bucket == stash.bucket && p.fingerprint == stash.fingerprint) return true;
    Bucket& b = data[p.bucket];
    for (int i = 0; i < kBuckets; ++i) {
      // Too branchy, slows things down
      // if (b[i].tail == 0) break;
      if (b[i].fingerprint == p.fingerprint && IsPrefixOf(b[i].tail, p.tail)) {
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

struct ElasticFilter {
  INLINE static const char* Name() {
    thread_local const constexpr char result[] = "Elastic";
    return result;
  }

 protected:
  detail::Side left, right;
  uint64_t log_side_size;
  detail::PcgRandom rng = {detail::kLogBuckets};
  const uint64_t* entropy;

 public:
  uint64_t occupied = 0;

  ElasticFilter(ElasticFilter&&);
  ElasticFilter(const ElasticFilter& that)
      : left(detail::Side(that.log_side_size, that.entropy)),
        right(detail::Side(that.log_side_size, that.entropy + 6)),
        log_side_size(that.log_side_size),
        rng(that.rng),
        entropy(that.entropy),
        occupied(that.occupied) {
    memcpy(left.data, that.left.data, sizeof(detail::Bucket) << that.log_side_size);
    memcpy(right.data, that.right.data, sizeof(detail::Bucket) << that.log_side_size);
  }
  ElasticFilter& operator=(const ElasticFilter& that) {
    this->~ElasticFilter();
    new (this) ElasticFilter(that);
    return *this;
  }

  uint64_t SizeInBytes() const {
    return 2 /* sides */ *
           (sizeof(detail::Path) /* stash */ +
            sizeof(detail::Slot) * (1 << log_side_size) * detail::kBuckets);
  }

 protected:
  // Verifies the occupied field:
  uint64_t Count() const {
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

 public:
  void Print() const {
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

  ElasticFilter(int log_side_size, const uint64_t* entropy)
      : left(log_side_size, entropy),
        right(log_side_size, entropy + 6),
        log_side_size(log_side_size),
        entropy(entropy) {}

  static ElasticFilter CreateWithBytes(uint64_t bytes) {
    thread_local constexpr const uint64_t kEntropy[13] = {
        0x2ba7538ee1234073, 0xfcc3777539b147d6, 0x6086c563576347e7, 0x52eff34ee1764465,
        0x8639cbf57f264867, 0x5a31ee34f0224ccb, 0x07a1cb8140744ee6, 0xf2296cf6a6524e9f,
        0x28a31cec9f6d4484, 0x688f3fe9de7245f6, 0x1dc17831966b41a2, 0xf227166e425e4b0c,
        0x15ab11b1a6bf4ea8};
    return ElasticFilter(8, kEntropy);
    return ElasticFilter(log(bytes / 2 / detail::kBuckets) / log(2), kEntropy);
   }

  INLINE bool FindHash(uint64_t k) const {
    return left.Find(detail::ToPath(k, left.f, log_side_size)) ||
           right.Find(detail::ToPath(k, right.f, log_side_size));
  }


  INLINE void InsertHash(uint64_t k) {
    // 95% is achievable, generally,but give it some room
    if (occupied > 0.94 * (1ul << log_side_size) * 2 * detail::kBuckets) {
      Upsize();
    }
    Insert(detail::ToPath(k, left.f, log_side_size));
  }

 protected:
  // After Stashed result, HT is close to full and should be upsized
  enum class InsertResult { Ok, Stashed, Failed };

  // After ttl, stash the input and return Stashed. Pre-condition: at least one stash is
  // empty. Also, p is a left path, not a right one.
  INLINE InsertResult Insert(detail::Path p, int ttl) {
    if (left.stash.tail != 0 && right.stash.tail != 0) return InsertResult::Failed;
    while (true) {
      detail::Path q = p;
      p = left.Insert(p, rng);
      if (p.tail == 0) {
        // Found an empty slot
        ++occupied;
        return InsertResult::Ok;
      }
      if (p == q) {
        // Combined with or already present in a slot. Success, but no increase in filter
        // size
        return InsertResult::Ok;
      }
      auto tail = p.tail;
      if (ttl <= 0 && left.stash.tail == 0) {
        // we've run out of room. If there's room in this stash, stash it here. If there
        // is not room in this stash, there must be room in the other, based on the
        // pre-condition for this method.
        left.stash = p;
        ++occupied;
        return InsertResult::Stashed;
      }
      // translate p to beign a path about the right half of the table
      p = detail::ToPath(detail::FromPathNoTail(p, left.f, log_side_size), right.f, log_side_size);
      // P's tail is now artificiall 0, but it should stay the same as we move from side to side
      p.tail = tail;

      // Now mimic the above.
      // TODO: avoid the code_duplication
      q = p;
      p = right.Insert(p, rng);
      if (p.tail == 0) {
        ++occupied;
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
        return InsertResult::Stashed;
      }
      --ttl;
      p = detail::ToPath(detail::FromPathNoTail(p, right.f, log_side_size), left.f,
                         log_side_size);
      p.tail = tail;
    }
  }

  // This method just increases ttl until insert succeeds.
  // TODO: upsize when insert fails with high enough ttl?
  INLINE InsertResult Insert(detail::Path q) {
    int ttl = 32;
    //If one stash is empty, we're fine. Only go here if both stashes are full
    while (left.stash.tail != 0 && right.stash.tail != 0) {
      ttl = 2 * ttl;
      // remove stash value
      detail::Path p = left.stash;
      left.stash.tail = 0;
      --occupied;
      // Insert it. We know this will succeed, as one stash is now empty, but we hope it
      // succeeds with Ok, not Stashed.
      InsertResult result = Insert(p, ttl);
      assert(result != InsertResult::Failed);
      // At least one stash is now free. Can now do the REAL insert (after the loop)
      if (result == InsertResult::Ok) break;

      // same as above, though now need to convert right stash to a left path, since
      // Insert takes only left paths.
      p = right.stash;
      uint64_t tail = p.tail;
      right.stash.tail = 0;
      --occupied;
      p = detail::ToPath(detail::FromPathNoTail(p, right.f, log_side_size), left.f,
                         log_side_size);
      p.tail = tail;
      result = Insert(p, ttl);
      assert(result != InsertResult::Failed);
      if (result == InsertResult::Ok) break;
    }
    // Can totally punt here. IOW, the stashes are ALWAYS full, since we don't even try
    // very hard not to fill them!
    return Insert(q, 1);
  }

  friend void swap(ElasticFilter&, ElasticFilter&);

  // Take an item from slot sl with bucket index i, a filter u that sl is in, a side that
  // sl is in, and a filter to move sl to, does so, potentially inserting TWO items in t,
  // as described in the paper.
  INLINE void UpsizeHelper(detail::Slot sl, uint64_t i, ElasticFilter* u,
                           detail::Side ElasticFilter::*s, ElasticFilter& t) {
    if (sl.tail == 0) return;
    detail::Path p;
    static_cast<detail::Slot&>(p) = sl;
    p.bucket = i;
    uint64_t q = detail::FromPathNoTail(p, (u->*s).f, u->log_side_size);
    if (sl.tail == 1ul << detail::kTailSize) {
      // There are no tail bits left! Insert two values.
      // First, hash to the left side of the larger table.
      p = detail::ToPath(q, (t.left).f, t.log_side_size);
      // Still no tail! :-)
      p.tail = sl.tail;
      t.Insert(p);
      // change theraw value by just one bit: its last
      q |= (1ul << (64 - u->log_side_size - detail::kHeadSize - 1));
      p = detail::ToPath(q, (t.left).f, t.log_side_size);
      p.tail = sl.tail;
      t.Insert(p);
    } else {
      // steal a bit from the tail
      q |= static_cast<uint64_t>(sl.tail >> detail::kTailSize)
           << (64 - u->log_side_size - detail::kHeadSize - 1);
      detail::Path r = detail::ToPath(q, (t.left).f, t.log_side_size);
      r.tail = (sl.tail << 1);
      t.Insert(r);
    }
  }

  // Double the size of the filter
  void Upsize() {
    ElasticFilter t(1 + log_side_size, entropy);
    for (auto s : {&ElasticFilter::left, &ElasticFilter::right}) {
      detail::Path stash = (this->*s).stash;
      UpsizeHelper(stash, stash.bucket, this, s, t);
      for (unsigned i = 0; i < (1u << log_side_size); ++i) {
        for (int j = 0; j < detail::kBuckets; ++j) {
          detail::Slot sl = (this->*s)[i][j];
          UpsizeHelper(sl, i, this, s, t);
        }
      }
    }
    using std::swap;
    swap(*this, t);
  }
};

INLINE void swap(ElasticFilter& x, ElasticFilter& y) {
  using std::swap;
  swap(x.left, y.left);
  swap(x.right, y.right);
  swap(x.log_side_size, y.log_side_size);
  swap(x.rng, y.rng);
  swap(x.entropy, y.entropy);
  swap(x.occupied, y.occupied);
}

}  // namespace filter
