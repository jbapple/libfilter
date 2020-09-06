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

#include <immintrin.h>

#include "memory.h"             // for libfilter_region


// The basic types and operations in a block filter:

// An opaque structure type. API users do not need to delve.
typedef struct libfilter_block_struct libfilter_block;

// Given a number of distinct values and a goal false-positive probability, returns the
// size of the filter needed to achieve them.
uint64_t libfilter_block_bytes_needed(double ndv, double fpp);
// Essentially the inverse operation: returns the heap space used by the data in the
// filter
inline uint64_t libfilter_block_size_in_bytes(const libfilter_block *);

// Initializes a filter. Returns 0 on success and < 0 on error
int libfilter_block_init(uint64_t heap_space, libfilter_block *);
// Destroys a filter. Returns 0 on success and < 0 on error
int libfilter_block_destruct(libfilter_block *);
// Adds a hash value to the filter. The hash value is expected to be pseudorandom. Passing
// non pseudorandom values can increase the false positive probability to 100%. Do not do
// this.
inline void libfilter_block_add_hash(uint64_t hash, const libfilter_block *);
// Find a hash value to the filter, returning true if the value was added earlier, and, if
// the value was not added earlier, false with a probability dictated by the heap space
// usage and the number of distinct hash values that have been added. As in
// libfilter_block_add_hash, the hash value is expected to be pseudorandom.
inline bool libfilter_block_find_hash(uint64_t hash, const libfilter_block *);

// Lower-level operations:
inline void libfilter_block_add_hash_external(uint64_t hash, uint64_t num_buckets,
                                              uint32_t *);
inline bool libfilter_block_find_hash_external(uint64_t hash, uint64_t num_buckets,
                                               const uint32_t *);
double libfilter_block_fpp(double ndv, double bytes);
uint64_t libfilter_block_capacity(uint64_t bytes, double fpp);
void libfilter_block_zero_out(libfilter_block *);
inline void libfilter_block_scalar_add_hash(uint64_t hash, const libfilter_block *);
inline bool libfilter_block_scalar_find_hash(uint64_t hash, const libfilter_block *);
inline void libfilter_block_scalar_add_hash_external(uint64_t hash, uint64_t num_buckets,
                                                     uint32_t *);
inline bool libfilter_block_scalar_find_hash_external(uint64_t hash, uint64_t num_buckets,
                                                      const uint32_t *);
#if defined(__AVX2__)
inline void libfilter_block_simd_add_hash(uint64_t hash, const libfilter_block *);
inline bool libfilter_block_simd_find_hash(uint64_t hash, const libfilter_block *);
inline void libfilter_block_simd_add_hash_external(uint64_t hash, uint64_t num_buckets,
                                                   uint32_t *);
inline bool libfilter_block_simd_find_hash_external(uint64_t hash, uint64_t num_buckets, const uint32_t *);
#endif  // __AVX2__

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

// TODO: write docs for this
libfilter_block libfilter_block_clone(const libfilter_block *);

__attribute__((visibility("hidden")))
__attribute__((always_inline)) inline uint64_t libfilter_block_index(
    const uint64_t hash, const uint64_t num_buckets) {
  return (((unsigned __int128)hash) * ((unsigned __int128)num_buckets)) >> 64;
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

__attribute__((always_inline)) inline void libfilter_block_scalar_add_hash_external(
    uint64_t hash, uint64_t num_buckets, uint32_t *data) {
  const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);
  const libfilter_block_scalar_bucket mask = libfilter_block_scalar_make_mask(hash);
  libfilter_block_scalar_bucket *bucket = (libfilter_block_scalar_bucket *)data;
  bucket += bucket_idx;
  for (unsigned i = 0; i < 8; ++i) {
    bucket->payload[i] = mask.payload[i] | bucket->payload[i];
  }
}

__attribute__((always_inline)) inline void libfilter_block_scalar_add_hash(
    uint64_t hash, const libfilter_block *here) {
  return libfilter_block_scalar_add_hash_external(hash, here->num_buckets_,
                                                  here->block_.block);
}

__attribute__((always_inline)) inline bool libfilter_block_scalar_find_hash_external(
    uint64_t hash, uint64_t num_buckets, const uint32_t *data) {
  const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);
  const libfilter_block_scalar_bucket mask = libfilter_block_scalar_make_mask(hash);
  const libfilter_block_scalar_bucket *bucket = (libfilter_block_scalar_bucket *)data;
  bucket += bucket_idx;
  for (unsigned i = 0; i < 8; ++i) {
    if (0 == (bucket->payload[i] & mask.payload[i])) return false;
  }
  return true;
}

__attribute__((always_inline)) inline bool libfilter_block_scalar_find_hash(
    uint64_t hash, const libfilter_block *here) {
  return libfilter_block_scalar_find_hash_external(hash, here->num_buckets_, here->block_.block);
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

// data might not be aligned at 32-byte boundary (definitely it at 4-byte aka 32-BIT
// boundary)
__attribute__((always_inline)) inline void libfilter_block_simd_add_hash_external(
    uint64_t hash, uint64_t num_buckets, uint32_t *data) {
  const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);
  //  printf("num_buckets: %lu, bucket_idx: %lu\n", num_buckets, bucket_idx);
  const __m256i mask = libfilter_block_simd_make_mask(hash);
  __m256i *bucket = (__m256i *)data;
  bucket += bucket_idx;
  //printf("libfilter_block_simd_add_hash_external\n");
  const __m256i bucketval = _mm256_loadu_si256(bucket);
  //printf("libfilter_block_simd_add_hash_external deref\n");
  _mm256_storeu_si256(bucket, _mm256_or_si256(bucketval, mask));
}

__attribute__((always_inline)) inline void libfilter_block_simd_add_hash(
    uint64_t hash, const libfilter_block *here) {
  const uint64_t bucket_idx = libfilter_block_index(hash, here->num_buckets_);
  const __m256i mask = libfilter_block_simd_make_mask(hash);
  __m256i *bucket = (__m256i *)here->block_.block;
  bucket += bucket_idx;
  _mm256_store_si256(bucket, _mm256_or_si256(*bucket, mask));
}

// data might not be aligned at 32-byte boundary (definitely it at 4-byte aka 32-BIT
// boundary)
__attribute__((always_inline)) inline bool libfilter_block_simd_find_hash_external(
    uint64_t hash, uint64_t num_buckets, const uint32_t *data) {
  const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);
  const __m256i mask = libfilter_block_simd_make_mask(hash);
  const __m256i *bucket = (const __m256i *)data;
  bucket += bucket_idx;
  return _mm256_testc_si256(_mm256_loadu_si256(bucket), mask);
}

__attribute__((always_inline)) inline bool libfilter_block_simd_find_hash(
    uint64_t hash, const libfilter_block *here) {
  const uint64_t bucket_idx = libfilter_block_index(hash, here->num_buckets_);
  const __m256i mask = libfilter_block_simd_make_mask(hash);
  const __m256i *bucket = (const __m256i *)here->block_.block;
  bucket += bucket_idx;
  return _mm256_testc_si256(*bucket, mask);
}

__attribute__((always_inline)) inline void libfilter_block_add_hash_external(
    uint64_t hash, uint64_t num_buckets, uint32_t *data) {
  //printf("libfilter_block_add_hash_external with simd\n");

  return libfilter_block_simd_add_hash_external(hash, num_buckets, data);
}

__attribute__((always_inline)) inline bool libfilter_block_find_hash_external(
    uint64_t hash, uint64_t num_buckets, const uint32_t *data) {
  return libfilter_block_simd_find_hash_external(hash, num_buckets, data);
}

#else
__attribute__((always_inline)) inline void libfilter_block_add_hash_external(
    uint64_t hash, uint64_t num_buckets, uint32_t *data) {
  return libfilter_block_scalar_add_hash_external(hash, num_buckets, data);
}

__attribute__((always_inline)) inline bool libfilter_block_find_hash_external(
    uint64_t hash, uint64_t num_buckets, const uint32_t *data) {
  return libfilter_block_scalar_find_hash_external(hash, num_buckets, data);
}
#endif

__attribute__((always_inline)) inline void libfilter_block_add_hash(
    uint64_t hash, const libfilter_block *here) {
  return libfilter_block_add_hash_external(hash, here->num_buckets_, here->block_.block);
}

__attribute__((always_inline)) inline bool libfilter_block_find_hash(
    uint64_t hash, const libfilter_block *here) {
  return libfilter_block_find_hash_external(hash, here->num_buckets_, here->block_.block);
}

#undef LIBFILTER_INTERNAL_HASH_SEEDS

// TODO: very fine-grained includes to use the SIMD instructions available even when not
// ALL are available.
