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
#include <new>
#include <utility>

namespace filter {

struct FrozenTaffyCuckoo {
  libfilter_frozen_taffy_cuckoo b;
  bool FindHash(uint64_t x) const {
    return libfilter_frozen_taffy_cuckoo_find_hash(&b, x);
  }

  size_t SizeInBytes() const { return libfilter_frozen_taffy_cuckoo_size_in_bytes(&b); }
  // bool InsertHash(uint64_t hash);

  INLINE static const char* Name() {
    thread_local const constexpr char result[] = "FrozenTaffyCuckoo";
    return result;
  }

  ~FrozenTaffyCuckoo() { libfilter_frozen_taffy_cuckoo_destruct(&b); }
  FrozenTaffyCuckoo(const FrozenTaffyCuckoo&) = delete;
  FrozenTaffyCuckoo& operator=(const FrozenTaffyCuckoo&) = delete;
  FrozenTaffyCuckoo& operator=(FrozenTaffyCuckoo&& that) {
    this->~FrozenTaffyCuckoo();
    new (this) FrozenTaffyCuckoo(std::move(that));
    return *this;
  }
  FrozenTaffyCuckoo(FrozenTaffyCuckoo&& that) {
    b = that.b;
    for (int i = 0; i < 2; ++i) {
      that.b.data_[i] = NULL;
      that.b.stash_[i] = NULL;
    }
  }
  FrozenTaffyCuckoo(libfilter_frozen_taffy_cuckoo&& that) {
    b = that;
    for (int i = 0; i < 2; ++i) {
      that.data_[i] = NULL;
      that.stash_[i] = NULL;
    }
  }
};

struct TaffyCuckooFilter {
  TaffyCuckooFilter(const TaffyCuckooFilter& that) {
    libfilter_taffy_cuckoo_clone(&that.b, &b);
  }
  TaffyCuckooFilter& operator=(const TaffyCuckooFilter& that) {
    this->~TaffyCuckooFilter();
    new (this) TaffyCuckooFilter(that);
    return *this;
  }
  TaffyCuckooFilter& operator=(TaffyCuckooFilter&& that) {
    this->~TaffyCuckooFilter();
    new (this) TaffyCuckooFilter(std::move(that));
    return *this;
  }
  TaffyCuckooFilter(TaffyCuckooFilter&& that) {
    b = that.b;
    for (int i = 0; i < 2; ++i) {
      that.b.sides[i].data = NULL;
      that.b.sides[i].stash = NULL;
    }
  }

  TaffyCuckooFilter(libfilter_taffy_cuckoo&& that) {
    b = that;
    for (int i = 0; i < 2; ++i) {
      that.sides[i].data = NULL;
      that.sides[i].stash = NULL;
    }
  }

  libfilter_taffy_cuckoo b;

  // TODO: change to int64_t and prevent negatives
  static TaffyCuckooFilter CreateWithBytes(size_t bytes) {
    return TaffyCuckooFilter{libfilter_taffy_cuckoo_create_with_bytes(bytes)};
  }

  static const char* Name() {
    thread_local const constexpr char result[] = "TaffyCuckoo";
    return result;
  }

  bool InsertHash(uint64_t h) { return libfilter_taffy_cuckoo_add_hash(&b, h); }
  bool FindHash(uint64_t h) const { return libfilter_taffy_cuckoo_find_hash(&b, h); }
  size_t SizeInBytes() const { return libfilter_taffy_cuckoo_size_in_bytes(&b); }
  FrozenTaffyCuckoo Freeze() const {
    return FrozenTaffyCuckoo{libfilter_taffy_cuckoo_freeze(&b)};
  }
  ~TaffyCuckooFilter() { libfilter_taffy_cuckoo_destruct(&b); }
};

TaffyCuckooFilter Union(const TaffyCuckooFilter& x, const TaffyCuckooFilter& y) {
  return {libfilter_taffy_cuckoo_union(&x.b, &y.b)};
}

}  // namespace filter
