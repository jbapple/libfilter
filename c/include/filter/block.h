// Copied from Apache Impala, then modified. Usable under the terms in the Apache License,
// Version 2.0.
//
// This is a block Bloom filter (from Putze et al.'s "Cache-, Hash- and Space-Efficient
// Bloom Filters") with some twists:
//
// 1. Each block is a split Bloom filter - see Section 2.1 of Broder and Mitzenmacher's
// "Network Applications of Bloom Filters: A Survey".
//
// 2. The number of bits set during each key addition is constant in order to take
// advantage of SIMD instructions.

#pragma once


#include <limits.h>             // for CHAR_BIT
#include <stdalign.h>           // for alignas
#include <stdbool.h>            // for bool, false, true
#include <stdint.h>             // for uint64_t

#if defined (__x86_64)
#include <immintrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include "memory.h"             // for libfilter_region


// The basic types and operations in a block filter:

// An opaque structure type. API users do not need to delve.
typedef struct libfilter_block_struct libfilter_block;

// Given a number of distinct values and a goal false-positive probability, returns the
// size of the filter needed to achieve them.
uint64_t libfilter_block_bytes_needed(double ndv, double fpp);

// Initializes a filter. Returns 0 on success and < 0 on error
int libfilter_block_init(uint64_t heap_space, libfilter_block *);
// Destroys a filter. Returns 0 on success and < 0 on error
int libfilter_block_destruct(libfilter_block *);
// Adds a hash value to the filter. The hash value is expected to be pseudorandom. Passing
// non pseudorandom values can increase the false positive probability to 100%. Do not do
// this.
inline void libfilter_block_add_hash(uint64_t hash, libfilter_block *);
// Find a hash value to the filter, returning true if the value was added earlier, and, if
// the value was not added earlier, false with a probability dictated by the heap space
// usage and the number of distinct hash values that have been added. As in
// libfilter_block_add_hash, the hash value is expected to be pseudorandom.
inline bool libfilter_block_find_hash(uint64_t hash, const libfilter_block *);
// TODO: write docs for this
int libfilter_block_clone(const libfilter_block *, libfilter_block*);

// Lower-level operations:
double libfilter_block_fpp(double ndv, double bytes);
uint64_t libfilter_block_capacity(uint64_t bytes, double fpp);
void libfilter_block_zero_out(libfilter_block *);
bool libfilter_block_equals(const libfilter_block *, const libfilter_block *);
void libfilter_block_serialize(const libfilter_block *, char *);
// returns < 0 on error
int libfilter_block_deserialize(uint64_t size_in_bytes, const char *from,
                                libfilter_block *to);
int libfilter_block_deserialize_from_ints(size_t n, const int32_t * from,
                                          libfilter_block *to);
// Essentially the inverse operation of libfilter_block_bytes_needed: returns the heap
// space used by the data in the filter
inline uint64_t libfilter_block_size_in_bytes(const libfilter_block *);

inline void libfilter_block_scalar_add_hash(uint64_t hash, libfilter_block *);
inline bool libfilter_block_scalar_find_hash(uint64_t hash, const libfilter_block *);
#if defined(__AVX2__) || defined(__ARM_NEON) || defined(__ARM_NEON__)
inline void libfilter_block_simd_add_hash(uint64_t hash, libfilter_block *);
inline bool libfilter_block_simd_find_hash(uint64_t hash, const libfilter_block *);
#endif

#if defined(LIBFILTER_BLOCK_SIMD)
#error "An exported feature macro cannot be defined"
#endif

#if defined(LIBFILTER_INTERNAL_HASH_SEEDS)
#error "An internal macro cannot be defined"
#endif

// TODO: allow user-specified hash seeds

#define LIBFILTER_INTERNAL_HASH_SEEDS                           \
  (long long)0x47b6137b44974d91, (long long)0x8824ad5ba2b7289d, \
      (long long)0x705495c72df1424b, (long long)0x9efc49475c6bfb31


struct libfilter_block_struct {
  uint64_t num_buckets_;
  libfilter_region block_;
};

__attribute__((visibility("hidden")))
__attribute__((always_inline)) inline uint64_t libfilter_block_index(
    const uint64_t hash, const uint32_t num_buckets) {
  return ((hash >> 32) * ((uint64_t)num_buckets)) >> 32;
}

typedef struct {
  alignas((8 * 32 / CHAR_BIT)) uint32_t payload[8];
} libfilter_block_scalar_bucket;

__attribute__((visibility("hidden")))
__attribute__((always_inline)) inline libfilter_block_scalar_bucket
libfilter_block_scalar_make_mask(uint64_t hash) {
  libfilter_block_scalar_bucket hash_data;
  const long long seeds[] = {LIBFILTER_INTERNAL_HASH_SEEDS};
  for (unsigned i = 0; i < (8 * 32 / CHAR_BIT) / sizeof(long long); ++i) {
    for (unsigned j = 0; j < sizeof(long long) / sizeof(uint32_t); ++j) {
      hash_data.payload[sizeof(long long) / sizeof(uint32_t) * i + j] =
          ((uint32_t)hash) * ((uint32_t)(seeds[i] >> (32 * j)));
    }
  }
  for (unsigned i = 0; i < 8; ++i) {
    hash_data.payload[i] = (hash_data.payload[i] >> (32 - 5));
  }
  for (unsigned i = 0; i < 8; ++i) {
    hash_data.payload[i] = (((uint32_t)1) << hash_data.payload[i]);
  }
  return hash_data;
}

__attribute__((always_inline)) inline void libfilter_block_scalar_add_hash(
    uint64_t hash, libfilter_block *here) {
  const uint64_t bucket_idx = libfilter_block_index(hash, here->num_buckets_);
  const libfilter_block_scalar_bucket mask =
      libfilter_block_scalar_make_mask(hash);
  libfilter_block_scalar_bucket *bucket =
      (libfilter_block_scalar_bucket *)here->block_.block;
  bucket += bucket_idx;
  for (unsigned i = 0; i < 8; ++i) {
    bucket->payload[i] = mask.payload[i] | bucket->payload[i];
  }
}

__attribute__((always_inline)) inline bool libfilter_block_scalar_find_hash(
    uint64_t hash, const libfilter_block *here) {
  const uint64_t bucket_idx = libfilter_block_index(hash, here->num_buckets_);
  const libfilter_block_scalar_bucket mask =
      libfilter_block_scalar_make_mask(hash);
  const libfilter_block_scalar_bucket *bucket =
      (libfilter_block_scalar_bucket *)here->block_.block;
  bucket += bucket_idx;
  for (unsigned i = 0; i < 8; ++i) {
    if (0 == (bucket->payload[i] & mask.payload[i])) return false;
  }
  return true;
}

__attribute__((always_inline)) inline uint64_t libfilter_block_size_in_bytes(
    const libfilter_block *here) {
  return (here->num_buckets_) * ((8 * 32 / CHAR_BIT));
}

// TODO: test 0xff = _mm256_test_epi32_mask for performance
// LIBFILTER_INTERNAL_DECL_SCALAR_BOTH(8, 32)

// TODO: replace all of these architecture-check macros by writing the code using
// assembly, not intrinsics.
#if defined(__AVX2__)
#define LIBFILTER_BLOCK_SIMD
__attribute__((always_inline)) inline __m256i libfilter_block_simd_make_mask(
    uint64_t hash) {
  const __m256i ones = _mm256_set1_epi32(1);
  const __m256i rehash = {LIBFILTER_INTERNAL_HASH_SEEDS};
  __m256i hash_data = _mm256_set1_epi32(hash);
  hash_data = _mm256_mullo_epi32(rehash, hash_data);
  hash_data = _mm256_srli_epi32(hash_data, 32 - 5);
  return _mm256_sllv_epi32(ones, hash_data);
}

__attribute__((always_inline)) inline void libfilter_block_simd_add_hash(
    uint64_t hash, libfilter_block *here) {
  const uint64_t bucket_idx = libfilter_block_index(hash, here->num_buckets_);
  const __m256i mask = libfilter_block_simd_make_mask(hash);
  __m256i * bucket = (__m256i*)here->block_.block;
  bucket += bucket_idx;
  _mm256_store_si256(bucket, _mm256_or_si256(*bucket, mask));
}

__attribute__((always_inline)) inline bool libfilter_block_simd_find_hash(
    uint64_t hash, const libfilter_block *here) {
  const uint64_t bucket_idx = libfilter_block_index(hash, here->num_buckets_);
  const __m256i mask = libfilter_block_simd_make_mask(hash);
  const __m256i *bucket = (const __m256i *)here->block_.block;
  bucket += bucket_idx;
  return _mm256_testc_si256(*bucket, mask);
}

__attribute__((always_inline)) inline void libfilter_block_add_hash(
    uint64_t hash, libfilter_block *here) {
  return libfilter_block_simd_add_hash(hash, here);
}

__attribute__((always_inline)) inline bool libfilter_block_find_hash(
    uint64_t hash, const libfilter_block *here) {
  return libfilter_block_simd_find_hash(hash, here);
}

#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#define LIBFILTER_BLOCK_SIMD

typedef struct {
  uint32x4_t payload[2];
} uint32x8_t;

__attribute__((always_inline)) inline uint32x8_t libfilter_block_simd_make_mask(
    uint64_t hash) {
  const uint32x4_t ones = vmovq_n_u32(1);
  uint32_t x[2][4] = {{0x47b6137b, 0x44974d91, 0x8824ad5b, 0xa2b7289d},
                      {0x705495c7, 0x2df1424b, 0x9efc4947, 0x5c6bfb31}};
  uint32x8_t rehash;
  rehash.payload[0] = vld1q_u32(x[0]);
  rehash.payload[1] = vld1q_u32(x[1]);
  uint32x8_t hash_data;
  hash_data.payload[0] = vmovq_n_u32(hash);
  hash_data.payload[1] = vmovq_n_u32(hash);

  hash_data.payload[0] = vmulq_u32(hash_data.payload[0], rehash.payload[0]);
  hash_data.payload[1] = vmulq_u32(hash_data.payload[1], rehash.payload[1]);

  hash_data.payload[0] = vshrq_n_u32(hash_data.payload[0], 32 - 5);
  hash_data.payload[1] = vshrq_n_u32(hash_data.payload[1], 32 - 5);

  hash_data.payload[0] = vshlq_u32(ones, (int32x4_t)hash_data.payload[0]);
  hash_data.payload[1] = vshlq_u32(ones, (int32x4_t)hash_data.payload[1]);
  return hash_data;
}

__attribute__((always_inline)) inline void libfilter_block_simd_add_hash(
    uint64_t hash, libfilter_block *here) {
  const uint64_t bucket_idx = libfilter_block_index(hash, here->num_buckets_);
  const uint32x8_t mask = libfilter_block_simd_make_mask(hash);
  uint32_t *bucket = here->block_.block;
  bucket += bucket_idx * 8;
  uint32x8_t real_bucket;
  real_bucket.payload[0] = vld1q_u32(&bucket[0]);
  real_bucket.payload[1] = vld1q_u32(&bucket[4]);
  uint32x8_t tmp;
  tmp.payload[0] = vorrq_u32(real_bucket.payload[0], mask.payload[0]);
  tmp.payload[1] = vorrq_u32(real_bucket.payload[1], mask.payload[1]);
  vst1q_u32(&bucket[0], tmp.payload[0]);
  vst1q_u32(&bucket[4], tmp.payload[1]);
}

__attribute__((always_inline)) inline bool libfilter_block_simd_find_hash(
    uint64_t hash, const libfilter_block *here) {
  const uint64_t bucket_idx = libfilter_block_index(hash, here->num_buckets_);
  const uint32x8_t mask = libfilter_block_simd_make_mask(hash);
  uint32_t *bucket = here->block_.block;
  bucket += bucket_idx * 8;
  uint32x8_t real_bucket;
  real_bucket.payload[0] = vld1q_u32(&bucket[0]);
  real_bucket.payload[1] = vld1q_u32(&bucket[4]);

  uint32x4_t out0 = vandq_u32(real_bucket.payload[0], mask.payload[0]);
  uint32x4_t out1 = vandq_u32(real_bucket.payload[1], mask.payload[1]);
  return vminvq_u32(out0) && vminvq_u32(out1);
}

__attribute__((always_inline)) inline void libfilter_block_add_hash(
    uint64_t hash, libfilter_block *here) {
  return libfilter_block_simd_add_hash(hash, here);
}

__attribute__((always_inline)) inline bool libfilter_block_find_hash(
    uint64_t hash, const libfilter_block *here) {
  return libfilter_block_simd_find_hash(hash, here);
}
#else
__attribute__((always_inline)) inline void libfilter_block_add_hash(
    uint64_t hash, libfilter_block *here) {
  return libfilter_block_scalar_add_hash(hash, here);
}

__attribute__((always_inline)) inline bool libfilter_block_find_hash(
    uint64_t hash, const libfilter_block *here) {
  return libfilter_block_scalar_find_hash(hash, here);
}

#endif

#undef LIBFILTER_INTERNAL_HASH_SEEDS

// TODO: very fine-grained includes to use the SIMD instructions available even when not
// ALL are available.
