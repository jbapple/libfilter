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
//#include "filter/paths.hpp"

extern "C" {
#include "filter/paths.h"
#include "filter/taffy-cuckoo.h"
#include "filter/util.h"
#include "filter/minimal-taffy-cuckoo.h"
}

namespace filter {

struct MinimalTaffyCuckooFilter {
  libfilter_minimal_taffy_cuckoo data;

  INLINE static const char* Name() {
    thread_local const constexpr char result[] = "MinTaffy";
    return result;
  }

  MinimalTaffyCuckooFilter(MinimalTaffyCuckooFilter&& that) : data(that.data) {
    libfilter_minimal_taffy_cuckoo_null_out(&that.data);
  }
  //MinimalTaffyCuckooFilter(libfilter_minimal_taffy_cuckoo&& b) : data(std::move(b)) {}
  ~MinimalTaffyCuckooFilter() { libfilter_minimal_taffy_cuckoo_destruct(&data); }
  MinimalTaffyCuckooFilter(const MinimalTaffyCuckooFilter&) = delete;
  MinimalTaffyCuckooFilter(libfilter_minimal_taffy_cuckoo&& steal) : data(steal) {
    libfilter_minimal_taffy_cuckoo_null_out(&steal);
  }

  uint64_t Capacity() const { return libfilter_minimal_taffy_cuckoo_capacity(&data); }

  uint64_t SizeInBytes() const {
    return libfilter_minimal_taffy_cuckoo_size_in_bytes(&data);
  }

  // Verifies the occupied field:
  // uint64_t Count() const { return data.Count(); }

  // void Print() const {
  //   for (int s = 0; s < 2; ++s) {
  //     for (auto stash : sides[s].stashes) {
  //       stash.Print();
  //       std::cout << std::endl;
  //     }
  //     for (unsigned k = 0; k < kLevels; ++k) {
  //       for (uint64_t i = 0; i < (1ull + (k < cursor)) << log_side_size; ++i) {
  //         for (int j = 0; j < kBuckets; ++j) {
  //           if (sides[s][k][i].data[j].tail) {
  //             sides[s][k][i].data[j].Print();
  //             std::cout << std::endl;
  //           }
  //         }
  //       }
  //     }
  //   }
  // }

  MinimalTaffyCuckooFilter(int log_side_size, const uint64_t* entropy)
      : data(libfilter_minimal_taffy_cuckoo_create(log_side_size, entropy)) {}

  static MinimalTaffyCuckooFilter CreateWithBytes(uint64_t bytes) {
    return MinimalTaffyCuckooFilter{
        libfilter_minimal_taffy_cuckoo_create_with_bytes(bytes)};
  }

  INLINE bool FindHash(uint64_t k) const {
    return libfilter_minimal_taffy_cuckoo_find_hash(&data, k);
  }
  INLINE bool InsertHash(uint64_t k) {
    return libfilter_minimal_taffy_cuckoo_add_hash(&data, k);
  }
};

}  // namespace filter
