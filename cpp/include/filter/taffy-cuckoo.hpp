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

extern "C" {
#include "filter/taffy-cuckoo.h"
}

#include <cstdint>

namespace filter {

struct FrozenTaffyCuckoo {
  FrozenTaffyCuckooBase b;
  bool FindHash(uint64_t x) const { return FrozenFindHash(&b, x); }

  size_t SizeInBytes() const { return FrozenSizeInBytes(&b); }
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
    return TaffyCuckooFilter{BaseCreateWithBytes(bytes)};
  }

  static const char* Name() {
    thread_local const constexpr char result[] = "TaffyCuckoo";
    return result;
  }

  bool InsertHash(uint64_t h) { return BaseInsertHash(&b, h); }
  bool FindHash(uint64_t h) const { return BaseFindHash(&b, h); }
  size_t SizeInBytes() const { return TaffyCuckooSizeInBytes(&b); }
  FrozenTaffyCuckoo Freeze() const { return {BaseFreeze(&b)}; }
  ~TaffyCuckooFilter() { TaffyCuckooFilterBaseDestroy(&b); }
};

TaffyCuckooFilter Union(const TaffyCuckooFilter& x, const TaffyCuckooFilter& y) {
  return {UnionTwo(&x.b, &y.b)};
}

}  // namespace filter
