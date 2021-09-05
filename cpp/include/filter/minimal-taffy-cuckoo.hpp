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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

#include "util.hpp"
#include "paths.hpp"

namespace filter {

namespace detail {

namespace minimal_taffy {

struct Bucket {
  Slot data[kBuckets] = {};
  INLINE Slot& operator[](uint64_t x) { return data[x]; }
  INLINE const Slot& operator[](uint64_t x) const { return data[x]; }
};

static_assert(sizeof(Bucket) == sizeof(Slot) * kBuckets, "sizeof(Bucket)");

struct Level {
  Bucket* data;

  INLINE Level(int log_level_size = 0)
      : data(new Bucket[1ul << log_level_size]()) {
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
        // This has a negligible effect on fpp
        // auto c = Combinable(b[i].tail, p.tail);
        // if (c > 0) {
        //   b[i].tail = c;
        //   return p;
        // }
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

void swap(Level& x, Level& y) {
  using std::swap;
  swap(x.data, y.data);
}

// A cuckoo hash table is made up of sides (in some incarnations), rather than jut two
// buckets from one array. Each side has a stash that holds any paths that couldn't fit.
// This is useful for random-walk cuckoo hashing, in which the leftover path needs a place
// to be stored so it doesn't invalidate old inserts.
struct Side {
  // TODO: can use identity for one side?
  Feistel hi, lo;
  Level levels[kLevels];
  std::vector<Path> stashes;

  INLINE Side(int log_level_size, const uint64_t* keys)
    : hi(&keys[0]), lo(&keys[6]), stashes() {
    for (uint64_t i = 0; i < kLevels; ++i) {
      levels[i].~Level();
      new (&levels[i]) Level(log_level_size);
    }
  }

  INLINE Level& operator[](unsigned i) {
    assert(i < detail::minimal_taffy::kLevels);
    return levels[i];
  }
  INLINE const Level& operator[](unsigned i) const { return levels[i]; }

  // Returns an empty path (tail = 0) if insert added a new element. Returns p if insert
  // succeded without adding anything new. Returns something else if that something else
  // was displaced by the insert. That item must be inserted then.
  INLINE Path Insert(Path p, PcgRandom& rng) {
    auto result = levels[p.level].Insert(p, rng);
    assert(Find(p));
    return result;
  }

  INLINE bool Find(Path p) const {
    for (auto stash : stashes) {
      if (stash.tail != 0 && stash.long_fp == p.long_fp &&
          stash.fingerprint == p.fingerprint && IsPrefixOf(stash.tail, p.tail) &&
          stash.level == p.level && stash.bucket == p.bucket) {
        return true;
      }
    }
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
  swap(x.stashes, y.stashes);
}

}  // namespace minimal_taffy
}  // namespace detail

struct MinimalTaffyCuckooFilter {
  INLINE static const char* Name() {
    thread_local const constexpr char result[] = "MinTaffy";
    return result;
  }

 public:
  detail::minimal_taffy::Side sides[2];
  uint64_t cursor;
  uint64_t log_side_size;
  detail::PcgRandom rng = {detail::minimal_taffy::kLogBuckets};
  const uint64_t* entropy;

 public:
  uint64_t occupied = 0;

  MinimalTaffyCuckooFilter(MinimalTaffyCuckooFilter&&);

  uint64_t Capacity() const {
    return 2 + 2 * detail::minimal_taffy::kBuckets *
                   ((1ul << log_side_size) * detail::minimal_taffy::kLevels +
                    (1ul << log_side_size) * cursor);
  }

  uint64_t SizeInBytes() const {
    return sizeof(detail::minimal_taffy::Slot) * Capacity() +
           2 * (sizeof(detail::minimal_taffy::Path) -
                sizeof(detail::minimal_taffy::Slot));
  }

 protected:
  // Verifies the occupied field:
  uint64_t Count() const {
    uint64_t result = 0;
    for (int s = 0; s < 2; ++s) {
      result += sides[s].stashes.size();
      for (unsigned k = 0; k < detail::minimal_taffy::kLevels; ++k) {
        for (uint64_t i = 0; i < (1ull + (k < cursor)) << log_side_size; ++i) {
          for (int j = 0; j < detail::minimal_taffy::kBuckets; ++j) {
            if (sides[s][k][i][j].tail != 0) ++result;
          }
        }
      }
    }
    return result;
  }

  void Print() const {
    for (int s = 0; s < 2; ++s) {
      for (auto stash : sides[s].stashes) {
        stash.Print();
        std::cout << std::endl;
      }
      for (unsigned k = 0; k < detail::minimal_taffy::kLevels; ++k) {
        for (uint64_t i = 0; i < (1ull + (k < cursor)) << log_side_size; ++i) {
          for (int j = 0; j < detail::minimal_taffy::kBuckets; ++j) {
            if (sides[s][k][i][j].tail) {
              sides[s][k][i][j].Print();
              std::cout << std::endl;
            }
          }
        }
      }
    }
  }

 public:
  MinimalTaffyCuckooFilter(int log_side_size, const uint64_t* entropy)
      : sides{{log_side_size, entropy}, {log_side_size, entropy + 12}},
        cursor(0),
        log_side_size(log_side_size) {
    Print();
  }

  static MinimalTaffyCuckooFilter CreateWithBytes(uint64_t /*bytes*/) {
    thread_local constexpr const uint64_t kEntropy[24] = {
        0x2ba7538ee1234073, 0xfcc3777539b147d6, 0x6086c563576347e7, 0x52eff34ee1764465,
        0x8639cbf57f264867, 0x5a31ee34f0224ccb, 0x07a1cb8140744ee6, 0xf2296cf6a6524e9f,
        0x28a31cec9f6d4484, 0x688f3fe9de7245f6, 0x1dc17831966b41a2, 0xf227166e425e4b0c,
        0x4a2a62bafc694440, 0x2e6bbea775e3429d, 0x5687dd060ba64169, 0xc5d95e8a38a44789,
        0xd30480ab74084edc, 0xd72483670ec14df3, 0x0414954940374787, 0x8cd86adfda93493f,
        0x50d61c3272a24ccb, 0x40cb1e4f0da34cc3, 0xb88f09c3af35472e, 0x8de6d01bb8a849a5};

    // TODO: start with a size other than 0
    return MinimalTaffyCuckooFilter(
        0,
        // std::max(1.0, log(1.0 * bytes / 2 / detail::minimal_taffy::kBuckets /
        //                   sizeof(detail::minimal_taffy::Slot) /
        //                   detail::minimal_taffy::kBuckets /
        //                   detail::minimal_taffy::kLevels) /
        //                   log(2)),
        kEntropy);
   }

  INLINE bool FindHash(uint64_t k) const {
    if (debug_lookup) {
      std::cout << sides[0].stashes.size() + sides[1].stashes.size() << std::endl;
    }
    for (int i : {0, 1}) {
       auto p = detail::minimal_taffy::ToPath(k, sides[i].lo, cursor, log_side_size, true);

      if (debug_lookup) std::cout << std::boolalpha << (p.tail != 0) << std::endl;
      if (p.tail != 0 && sides[i].Find(p)) {
        debug_lookup = false;
        return true;
      }
      p = detail::minimal_taffy::ToPath(k, sides[i].hi, cursor, log_side_size, false);
      if (debug_lookup) std::cout << std::boolalpha << (p.tail != 0) << std::endl;
      if (p.tail != 0 && sides[i].Find(p)) {
        debug_lookup = false;
        return true;
      }
    }
    debug_lookup = false;
    return false;
  }

uint64_t inserted_hashes = 0;
mutable bool debug_lookup = false;

  INLINE bool InsertHash(uint64_t k) {
    ++inserted_hashes;

    while (occupied > 0.9 * Capacity() || occupied + 4 >= Capacity() ||
        sides[0].stashes.size() + sides[1].stashes.size() > 8) {
      Upsize();
    }
    // TODO: only need one path here. Which one to pick?
    auto p =
        detail::minimal_taffy::ToPath(k, sides[0].hi, cursor, log_side_size, false);
    Insert(0, p, 128);
    return true;
  }

  enum class InsertResult { Ok, Stashed, Failed };

  inline InsertResult Insert(int side, detail::minimal_taffy::Path p, int ttl) {
    assert(p.tail != 0);
    while (true) {
      for (int i : {side, 1 - side}) {
        --ttl;
        if (ttl < 0) {
          sides[i].stashes.push_back(p);
          ++occupied;
          return InsertResult::Stashed;
        }
        detail::minimal_taffy::Path q = p;
        detail::minimal_taffy::Path r = sides[i].Insert(p, rng);
        if (r.tail == 0) {
          // Found an empty slot
          ++occupied;
          return InsertResult::Ok;
        }
        if (r == q) {
          // Combined with or already present in a slot. Success, but no increase in
          // filter size
          return InsertResult::Ok;
        }
        detail::minimal_taffy::Path extra;
        auto next = RePath(r, sides[i].lo, sides[i].hi, sides[1 - i].lo, sides[1 - i].hi,
                           log_side_size, log_side_size, cursor, cursor, &extra);
        if (extra.tail != 0) {
          Insert(1 - i, extra, ttl);
        }
        // TODO: what if insert returns stashed? Do we need multiple states? Maybe green , yellow red?
        // Or maybe break at the beginning of this logic if repath returns two things
        // or retry if there aren't two stashes open at this time.
        p = next;
        assert(p.tail != 0);
      }
    }
  }

  friend void swap(MinimalTaffyCuckooFilter&, MinimalTaffyCuckooFilter&);

  // Double the size of one level of the filter
  void Upsize() {
    detail::minimal_taffy::Bucket* last_data[2] = {sides[0][cursor].data, sides[1][cursor].data};
    {
      detail::minimal_taffy::Bucket* next[2];
      next[0] = new detail::minimal_taffy::Bucket[2 << log_side_size]();
      next[1] = new detail::minimal_taffy::Bucket[2 << log_side_size]();
      sides[0][cursor].data = next[0];
      sides[1][cursor].data = next[1];
    }
    cursor = cursor + 1;
    detail::minimal_taffy::Path p;
    p.level = cursor - 1;

    std::vector<detail::minimal_taffy::Path> stashes[2] = {
        std::vector<detail::minimal_taffy::Path>(),
        std::vector<detail::minimal_taffy::Path>()};
    using std::swap;
    swap(stashes[0], sides[0].stashes);
    swap(stashes[1], sides[1].stashes);
    occupied = occupied - stashes[0].size();
    occupied = occupied - stashes[1].size();
    sides[0].stashes.clear();
    sides[1].stashes.clear();
    for (int s : {0, 1}) {
      for (auto stash : stashes[s]) {
        detail::minimal_taffy::Path q, r;
        r = RePathUpsize(stash, sides[s].lo, sides[s].hi, log_side_size, cursor - 1, &q);
        auto ttl = 128;
        assert(r.tail != 0);
        if (q.tail != 0) {
          Insert(s, q, ttl);
        }
        Insert(s, r, ttl);
      }
    }
    for (int s: {0,1}) {
      for (unsigned i = 0; i < (1u << log_side_size); ++i) {
        p.bucket = i;
        for (int j = 0; j < detail::minimal_taffy::kBuckets; ++j) {
          if (last_data[s][i][j].tail == 0) continue;
          --occupied;
          p.SetSlot(last_data[s][i][j]);
          assert(p.tail != 0);
          detail::minimal_taffy::Path q, r;
          r = RePathUpsize(p, sides[s].lo, sides[s].hi, log_side_size, cursor - 1, &q);
          auto ttl = 128;
          assert(r.tail != 0);
          if (q.tail != 0) {
            Insert(s, q, ttl);
          }
          Insert(s, r, ttl);
        }
      }
    }

    if (cursor == detail::minimal_taffy::kLevels) {
      cursor = 0;
      ++log_side_size;
      using namespace std;
      for (int i : {0, 1}) swap(sides[i].lo, sides[i].hi);
    }
  }
};

INLINE void swap(MinimalTaffyCuckooFilter& x, MinimalTaffyCuckooFilter& y) {
  using std::swap;
  swap(x.sides[0], y.sides[0]);
  swap(x.sides[1], y.sides[1]);
  swap(x.log_side_size, y.log_side_size);
  swap(x.rng, y.rng);
  swap(x.entropy, y.entropy);
  swap(x.occupied, y.occupied);
}

}  // namespace filter
