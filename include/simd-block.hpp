// C++ wrapper around simd-block.h.

#pragma once

extern "C" {
#include "simd-block.h"
#include "util.h"
}

#include <cstdint>
#include <exception>
#include <random>
#include <stdexcept>

// TODO: push & pop macros so we don't clobber existing ones

namespace filter {

namespace detail {

// The concatenation operator only works on macro parameters, so macros like F2 are needed
// when things that are not parameters need to be concatenated. Macros like H2 are needed
// when one of the things to be concatenated is itself a macro call.
#define F2(P, Q) P##Q
#define H2(P, Q) F2(P, Q)

#define F3(P, Q, R) P##Q##R
#define H3(P, Q, R) F3(P, Q, R)

#define F4(P, Q, R, S) P##Q##R##S

#ifndef F6
#define F6(P, Q, R, S, T, U) P##Q##R##S##T##U
#endif

#define FILTER_TYPE(prefix, ARITY, WORD_BITS) F4(prefix, ARITY, x, WORD_BITS)
#define FILTER_TYPE_STRING(prefix, ARITY, WORD_BITS) #prefix "-" #ARITY "x" #WORD_BITS
#define METHOD(prefix, ARITY, WORD_BITS, x) \
  H3(FILTER_TYPE(prefix, ARITY, WORD_BITS), _, x)

#define SHINGLE_FILTER_TYPE(prefix, ARITY, WORD_BITS, st) \
  F6(prefix, ARITY, x, WORD_BITS, _, st)
#define SHINGLE_FILTER_TYPE_STRING(prefix, ARITY, WORD_BITS, st) \
  #prefix "-" #ARITY "x" #WORD_BITS "-" #st
#define SHINGLE_METHOD(prefix, ARITY, WORD_BITS, st, x) \
  H3(SHINGLE_FILTER_TYPE(prefix, ARITY, WORD_BITS, st), _, x)

template <int Arity, int WordBits>
class ScalarBlockFilterP;

template <int Arity, int WordBits, int Stride>
class ScalarShingleBlockFilterP;

template <int Arity, int WordBits>
class SimdBlockFilterP;

template <int Arity, int WordBits, int Stride>
class SimdShingleBlockFilterP;

#define DECL_BASE(PREFIX, ARITY, WORD_BITS, ADD, FIND, NAME_STRING)    \
 protected:                                                            \
  SimdBlockBloomFilter payload_;                                       \
  static constexpr int kHashBits = ARITY;                              \
  static constexpr int kBucketBytes = ARITY * WORD_BITS / CHAR_BIT;    \
  static constexpr int kBucketWords = ARITY;                           \
  using uint64_t = std::uint64_t;                                      \
                                                                       \
 public:                                                               \
  void InsertHash(uint64_t hash) { return ADD(hash, &payload_); }      \
                                                                       \
  bool FindHash(uint64_t hash) const { return FIND(hash, &payload_); } \
                                                                       \
  uint64_t SizeInBytes() const {                                       \
    return METHOD(PREFIX, ARITY, WORD_BITS, size_in_bytes)(&payload_); \
  }                                                                    \
                                                                       \
  static constexpr char NAME[] = NAME_STRING

#define DECL_SSBFP(FLAVOR, PREFIX, ARITY, WORD_BITS, ST)                 \
  template <>                                                            \
  class H2(FLAVOR, ShingleBlockFilterP)<ARITY, WORD_BITS, ST> {          \
    DECL_BASE(PREFIX, ARITY, WORD_BITS,                                  \
              SHINGLE_METHOD(PREFIX, ARITY, WORD_BITS, ST, add_hash),    \
              SHINGLE_METHOD(PREFIX, ARITY, WORD_BITS, ST, find_hash),   \
              SHINGLE_FILTER_TYPE_STRING(PREFIX, ARITY, WORD_BITS, ST)); \
  }

#define DECL_SBFP(FLAVOR, PREFIX, ARITY, WORD_BITS)                                 \
  template <>                                                                       \
  class H2(FLAVOR, BlockFilterP)<ARITY, WORD_BITS> {                                \
    DECL_BASE(PREFIX, ARITY, WORD_BITS, METHOD(PREFIX, ARITY, WORD_BITS, add_hash), \
              METHOD(PREFIX, ARITY, WORD_BITS, find_hash),                          \
              FILTER_TYPE_STRING(PREFIX, ARITY, WORD_BITS));                        \
  }

#define DECL_MOST(FLAVOR, BASE, ARITY, WORD_BITS) \
  DECL_SBFP(FLAVOR, BASE, ARITY, WORD_BITS);      \
  DECL_SSBFP(FLAVOR, BASE, ARITY, WORD_BITS, 1);  \
  DECL_SSBFP(FLAVOR, BASE, ARITY, WORD_BITS, 2);  \
  DECL_SSBFP(FLAVOR, BASE, ARITY, WORD_BITS, 4);  \
  DECL_SSBFP(FLAVOR, BASE, ARITY, WORD_BITS, 8);  \
  DECL_SSBFP(FLAVOR, BASE, ARITY, WORD_BITS, 16)

#define DECL_MOST_SCALAR(ARITY, WORD_BITS) \
  DECL_MOST(Scalar, libfilter_block_scalar_, ARITY, WORD_BITS)
#define DECL_MOST_SIMD(ARITY, WORD_BITS) \
  DECL_MOST(Simd, libfilter_block_simd_, ARITY, WORD_BITS)

DECL_MOST_SCALAR(4, 32);

#if defined(LIBFILTER_SIMD_5_128)
DECL_MOST_SIMD(4, 32);
#endif

DECL_MOST_SCALAR(8, 32);
DECL_SSBFP(Scalar, libfilter_block_scalar_, 8, 32, 32);

#if defined(LIBFILTER_SIMD_5_256)
DECL_MOST_SIMD(8, 32);
DECL_SSBFP(Simd, libfilter_block_simd_, 8, 32, 32);
#endif

DECL_MOST_SCALAR(16, 32);
DECL_SSBFP(Scalar, libfilter_block_scalar_, 16, 32, 32);
DECL_SSBFP(Scalar, libfilter_block_scalar_, 16, 32, 64);

#if defined(LIBFILTER_SIMD_5_512)
DECL_MOST_SIMD(16, 32);
DECL_SSBFP(Simd, libfilter_block_simd_, 16, 32, 32);
DECL_SSBFP(Simd, libfilter_block_simd_, 16, 32, 64);
#endif

DECL_MOST_SCALAR(8, 64);
DECL_SSBFP(Scalar, libfilter_block_scalar_, 8, 64, 32);
DECL_SSBFP(Scalar, libfilter_block_scalar_, 8, 64, 64);

#if defined(LIBFILTER_SIMD_6_512)
DECL_MOST_SIMD(8, 64);
DECL_SSBFP(Simd, libfilter_block_simd_, 8, 64, 32);
DECL_SSBFP(Simd, libfilter_block_simd_, 8, 64, 64);
#endif

DECL_MOST_SCALAR(4, 64);
DECL_SSBFP(Scalar, libfilter_block_scalar_, 4, 64, 32);

#if defined(LIBFILTER_SIMD_6_256)
DECL_MOST_SIMD(4, 64);
DECL_SSBFP(Simd, libfilter_block_simd_, 4, 64, 32);
#endif

template <typename PARENT>
class GenericBF : public PARENT {
 protected:
  using PARENT::kBucketBytes;
  using PARENT::kBucketWords;
  using PARENT::kHashBits;

 public:
  ~GenericBF() { libfilter_block_destruct(kBucketBytes, &this->payload_); }

 protected:
  explicit GenericBF(uint64_t bytes) {
    if (0 != libfilter_block_init(bytes, kBucketBytes, &this->payload_)) {
      throw std::runtime_error("libfilter_block_init");
    }
  };

 public:
  static GenericBF CreateWithBytes(uint64_t bytes) { return GenericBF(bytes); }
};

template <typename GRAND_PARENT>
class SpecificBF : public GenericBF<GRAND_PARENT> {
 protected:
  using Parent = GenericBF<GRAND_PARENT>;
  using Parent::Parent;
  using Parent::kBucketBytes;
  using Parent::kBucketWords;
  using Parent::kHashBits;

 public:
  // TODO: why passing hash bits twice?
  static double FalsePositiveProbability(uint64_t ndv, uint64_t bytes) {
    return libfilter_block_fpp(ndv, bytes, kHashBits, kBucketWords, kHashBits);
  }

  static uint64_t MinSpaceNeeded(uint64_t ndv, double fpp) {
    return libfilter_block_bytes_needed(ndv, fpp, kHashBits, kBucketWords, kHashBits);
  }

  static uint64_t MaxCapacity(uint64_t bytes, double fpp) {
    return libfilter_block_capacity(bytes, fpp, kHashBits, kBucketWords, kHashBits);
  }

  using Parent::CreateWithBytes;

  static SpecificBF CreateWithNdvFpp(uint64_t ndv, double fpp) {
    const uint64_t bytes =
        libfilter_block_bytes_needed(ndv, fpp, kHashBits, kBucketWords, kHashBits);
    return CreateWithBytes(bytes);
  }
};
}  // namespace detail

template <int Arity, int WordBits>
using ScalarBlockFilter = detail::SpecificBF<detail::ScalarBlockFilterP<Arity, WordBits>>;

template <int Arity, int WordBits>
using SimdBlockFilter = detail::SpecificBF<detail::SimdBlockFilterP<Arity, WordBits>>;

template <int Arity, int WordBits>
struct BlockFilter : ScalarBlockFilter<Arity, WordBits> {};

template <int Arity, int WordBits, int Stride>
using SimdShingleBlockFilter =
    detail::GenericBF<detail::SimdShingleBlockFilterP<Arity, WordBits, Stride>>;

template <int Arity, int WordBits, int Stride>
using ScalarShingleBlockFilter =
    detail::GenericBF<detail::ScalarShingleBlockFilterP<Arity, WordBits, Stride>>;

template <int Arity, int WordBits, int Stride>
struct ShingleBlockFilter : ScalarShingleBlockFilter<Arity, WordBits, Stride> {};

#if defined(LIBFILTER_SIMD_5_256)
template <>
struct BlockFilter<8, 32> : SimdBlockFilter<8, 32> {};
template <int Stride>
struct ShingleBlockFilter<8, 32, Stride> : SimdShingleBlockFilter<8, 32, Stride> {};
#endif

#if defined(LIBFILTER_SIMD_5_512)
template <>
struct BlockFilter<16, 32> : SimdBlockFilter<16, 32> {};
template <int Stride>
struct ShingleBlockFilter<16, 32, Stride> : SimdShingleBlockFilter<16, 32, Stride> {};
#endif

#if defined(LIBFILTER_SIMD_5_128)
template <>
struct BlockFilter<4, 32> : SimdBlockFilter<4, 32> {};
template <int Stride>
struct ShingleBlockFilter<4, 32, Stride> : SimdShingleBlockFilter<4, 32, Stride> {};
#endif

#if defined(LIBFILTER_SIMD_6_256)
template <>
struct BlockFilter<4, 64> : SimdBlockFilter<4, 64> {};
template <int Stride>
struct ShingleBlockFilter<4, 64, Stride> : SimdShingleBlockFilter<4, 64, Stride> {};
#endif

#if defined(LIBFILTER_SIMD_6_512)
template <>
struct BlockFilter<8, 64> : SimdBlockFilter<8, 64> {};
template <int Stride>
struct ShingleBlockFilter<8, 64, Stride> : SimdShingleBlockFilter<8, 64, Stride> {};
#endif

}  // namespace filter
