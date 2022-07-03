#pragma once

extern "C" {
#include "filter/static.h"
}

#include <memory>

namespace filter {

struct StaticFilter {
 protected:
  libfilter_static payload_;

 public:
  StaticFilter(size_t n, const uint64_t* hashes)
      : payload_(libfilter_static_construct(n, hashes)) {}

  ~StaticFilter() { libfilter_static_destruct(payload_); }

  bool FindHash(uint64_t hash) { return libfilter_static_find_hash(payload_, hash); }

  StaticFilter(const StaticFilter& that)
      : payload_(libfilter_static_clone(that.payload_)) {}

  StaticFilter(StaticFilter&& that) : payload_(that.payload_) {
    that.payload_.length_ = 0;
    that.payload_.region_.block = nullptr;
    that.payload_.region_.to_free = nullptr;
  }

  StaticFilter& operator=(const StaticFilter& that) {
    this->~StaticFilter();
    new (this) StaticFilter(that);
    return *this;
  }

  StaticFilter& operator=(StaticFilter&& that) {
    this->~StaticFilter();
    new (this) StaticFilter(that);
    return *this;
  }

  static const char* Name() {
    static const char NAME[] = "StaticFilter";
    return NAME;
  }

  size_t SizeInBytes() const { return payload_.length_; }
};

}  // namespace filter
