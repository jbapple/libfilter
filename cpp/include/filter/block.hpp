// C++ wrapper around block.h.

#pragma once

extern "C" {
#include "filter/block.h"
}

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace filter {

namespace detail {

// TODO: sprinkle nothrows

class GenericBF {
 protected:
  libfilter_block payload_;
  using uint64_t = std::uint64_t;

 public:
  uint64_t SizeInBytes() const { return libfilter_block_size_in_bytes(&payload_); }

 public:
  ~GenericBF() {
    // TODO: this swallows an error when return value is negative
    libfilter_block_destruct(&this->payload_);
  }

 protected:
  explicit GenericBF(uint64_t bytes) {
    if (0 != libfilter_block_init(bytes, &this->payload_)) {
      throw std::runtime_error("libfilter_block_init");
    }
  };

  GenericBF(const GenericBF& that) : payload_(libfilter_block_clone(&that.payload_)) {}
  GenericBF& operator=(const GenericBF& that) {
    this->~GenericBF();
    payload_ = libfilter_block_clone(&that.payload_);
    return *this;
  }

 public:
  GenericBF(GenericBF&& that) : payload_(that.payload_) {
    libfilter_block_zero_out(&that.payload_);
  }

  // TODO: std::Swap all the types

  GenericBF& operator=(GenericBF&& that) {
    // TODO: do this in initializer? Needs to be a parent constructor, in that case.
    using std::swap;
    swap(this->payload_, that.payload_);
    return *this;
  }

  bool operator==(const GenericBF& that) const {
    return libfilter_block_equals(&this->payload_, &that.payload_);
  }

  // TODO: why passing hash bits twice?
  static double FalsePositiveProbability(uint64_t ndv, uint64_t bytes) {
    return libfilter_block_fpp(ndv, bytes);
  }

  static uint64_t MinSpaceNeeded(uint64_t ndv, double fpp) {
    return libfilter_block_bytes_needed(ndv, fpp);
  }

  static uint64_t MaxCapacity(uint64_t bytes, double fpp) {
    return libfilter_block_capacity(bytes, fpp);
  }

  static GenericBF CreateWithBytes(uint64_t bytes) {
    return GenericBF(bytes);
  }

  static GenericBF CreateWithNdvFpp(uint64_t ndv, double fpp) {
    const uint64_t bytes = libfilter_block_bytes_needed(ndv, fpp);
    return CreateWithBytes(bytes);
  }
};

template <void (*INSERT_HASH)(uint64_t, libfilter_block*),
          bool (*FIND_HASH)(uint64_t, const libfilter_block*)>
struct SpecificBF : GenericBF {
 public:
  bool InsertHash(uint64_t hash) { INSERT_HASH(hash, &payload_); return true; }
  bool FindHash(uint64_t hash) const { return FIND_HASH(hash, &payload_); }
  SpecificBF(GenericBF&& x) : GenericBF(std::move(x)) {}
  SpecificBF& operator=(GenericBF&& that) {
    (GenericBF&)*this = std::move(that);
    return *this;
  }
  static SpecificBF CreateWithBytes(uint64_t bytes) {
    return GenericBF::CreateWithBytes(bytes);
  }
  static SpecificBF CreateWithNdvFpp(uint64_t ndv, double fpp) {
    return GenericBF::CreateWithNdvFpp(ndv, fpp);
  }
  // SpecificBF(const SpecificBF&) = delete;
  // SpecificBF& operator=(const SpecificBF&) = delete;
};

}  // namespace detail

struct ScalarBlockFilter : detail::SpecificBF<libfilter_block_scalar_add_hash,
                                              libfilter_block_scalar_find_hash> {
  static const char* Name() {
    static const char NAME[] = "ScalarBlockFilter";
    return NAME;
  }
  using Parent = detail::SpecificBF<libfilter_block_scalar_add_hash,
                                    libfilter_block_scalar_find_hash>;
  ScalarBlockFilter(detail::GenericBF&& x) : Parent(std::move(x)) {}
  ScalarBlockFilter& operator=(GenericBF&& that) {
    (Parent&)* this = std::move(that);
    return *this;
  }
  static constexpr bool is_simd = false;
  using Scalar = ScalarBlockFilter;
  // ScalarBlockFilter(const ScalarBlockFilter&) = delete;
  // ScalarBlockFilter& operator=(const ScalarBlockFilter&) = delete;
  static ScalarBlockFilter CreateWithBytes(uint64_t bytes) {
    return GenericBF::CreateWithBytes(bytes);
  }
  static ScalarBlockFilter CreateWithNdvFpp(uint64_t ndv, double fpp) {
    return GenericBF::CreateWithNdvFpp(ndv, fpp);
  }
};

#if defined(LIBFILTER_BLOCK_SIMD)

struct SimdBlockFilter
    : detail::SpecificBF<libfilter_block_simd_add_hash, libfilter_block_simd_find_hash> {
  static const char* Name() {
    static const char NAME[] = "SimdBlockFilter";
    return NAME;
  }
  // static constexpr char NAME[] = "SimdBlockFilter";
  using Parent = detail::SpecificBF<libfilter_block_simd_add_hash, libfilter_block_simd_find_hash>;
  SimdBlockFilter(detail::GenericBF&& x) : Parent(std::move(x)) {}
  static constexpr bool is_simd = true;
  using Scalar = ScalarBlockFilter;
  // SimdBlockFilter(const SimdBlockFilter&) = delete;
  // SimdBlockFilter& operator=(const SimdBlockFilter&) = delete;
  SimdBlockFilter& operator=(GenericBF&& that) {
    (Parent&)* this = std::move(that);
    return *this;
  }
  static SimdBlockFilter CreateWithBytes(uint64_t bytes) {
    return GenericBF::CreateWithBytes(bytes);
  }
  static SimdBlockFilter CreateWithNdvFpp(uint64_t ndv, double fpp) {
    return GenericBF::CreateWithNdvFpp(ndv, fpp);
  }
};

using BlockFilter = SimdBlockFilter;

#else
using BlockFilter = ScalarBlockFilter;
#endif

}  // namespace filter
