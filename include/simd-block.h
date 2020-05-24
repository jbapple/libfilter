// @file
//
// Copied from Apache Impala, usable under the terms in the Apache License, Version 2.0.
//
// This is a block Bloom filter (from Putze et al.'s "Cache-, Hash- and Space-Efficient
// Bloom Filters") with some twists:
//
// 1. Each block is a split Bloom filter - see Section 2.1 of Broder and Mitzenmacher's
// "Network Applications of Bloom Filters: A Survey".
//
// 2. The number of bits set per Add() is contant in order to take advantage of SIMD
// instructions.

#pragma once

#include <limits.h>    // for CHAR_BIT
#include <stdalign.h>  // for alignas
#include <stdbool.h>   // for bool
#include <stdint.h>    // for uint64_t
#include <stdio.h>     // for printf
#include <string.h>    // for memset

#include "memory.h"  // for region_alloc_result, region, clear_region
#include "util.h"

// TODO: compile guards for different ISAs
#include <immintrin.h>

#pragma push_macro("LOG_WORD_BITS")
#pragma push_macro("BUCKET_BITS")
#pragma push_macro("WORD_BITS")
#pragma push_macro("HASH_SEEDS")
#pragma push_macro("TEST_BUCKET_MASK")

#define F2(P, Q) P##Q
#define H2(P, Q) F2(P, Q)
#define F3(P, Q, R) P##Q##R
#define H3(P, Q, R) F3(P, Q, R)
#define F4(P, Q, R, S) P##Q##R##S
#define H4(P, Q, R, S) F4(P, Q, R, S)
#define F6(P, Q, R, S, T, U) P##Q##R##S##T##U
#define H6(P, Q, R, S, T, U) F6(P, Q, R, S, T, U)
#define F8(P, Q, R, S, T, U, V, W) P##Q##R##S##T##U##V##W
#define H8(P, Q, R, S, T, U, V, W) F8(P, Q, R, S, T, U, V, W)

#define BUCKET_BYTES(ARITY, WORD_BITS) (ARITY * WORD_BITS / CHAR_BIT)
#define WORD_TYPE(WORD_BITS) H3(uint, WORD_BITS, _t)
#define DEFAULT_METHOD(ARITY, WORD_BITS, M) \
  H6(libfilter_block_, ARITY, x, WORD_BITS, _, M)
#define DEFAULT_SHINGLE_METHOD(ARITY, WORD_BITS, STRIDE, M) \
  H8(libfilter_block_, ARITY, x, WORD_BITS, _, STRIDE, _, M)

#define FRACTION_512_32 16
#define FRACTION_512_64 8
#define FRACTION_256_32 8
#define FRACTION_256_64 4
#define FRACTION_128_32 4

#define PRODUCT_16_32 512
#define PRODUCT_8_64 512
#define PRODUCT_8_32 256
#define PRODUCT_4_64 256
#define PRODUCT_4_32 128

#define LOG2_32 5
#define LOG2_64 6

#define POW2_5 32
#define POW2_6 64

#define SCALAR_BUCKET_TYPE(ARITY, WORD_BITS) \
  H4(libfilter_block_scalar_bucket_, ARITY, x, WORD_BITS)
#define SCALAR_FILTER_TYPE(ARITY, WORD_BITS) \
  H4(libfilter_block_scalar_, ARITY, x, WORD_BITS)
#define SCALAR_METHOD(ARITY, WORD_BITS, x) H3(SCALAR_FILTER_TYPE(ARITY, WORD_BITS), _, x)
#define SCALAR_SHINGLE_BUCKET_TYPE(ARITY, WORD_BITS, STRIDE) \
  H6(libfilter_block_scalar_shingle_bucket_, ARITY, x, WORD_BITS, _, STRIDE)
#define SCALAR_SHINGLE_FILTER_TYPE(ARITY, WORD_BITS, STRIDE) \
  H6(libfilter_block_scalar_, ARITY, x, WORD_BITS, _, STRIDE)
#define SCALAR_SHINGLE_METHOD(ARITY, WORD_BITS, STRIDE, x) \
  H3(SCALAR_SHINGLE_FILTER_TYPE(ARITY, WORD_BITS, STRIDE), _, x)

#define SHINGLE_SIMD_FILTER_TYPE(ARITY, WORD_BITS, STRIDE) \
  H6(libfilter_block_simd_, ARITY, x, WORD_BITS, _, STRIDE)
#define SHINGLE_SIMD_METHOD(ARITY, WORD_BITS, STRIDE, x) \
  H3(SHINGLE_SIMD_FILTER_TYPE(ARITY, WORD_BITS, STRIDE), _, x)
#define SIMD_BUCKET_TYPE(ARITY, WORD_BITS) H3(__m, H4(PRODUCT_, ARITY, _, WORD_BITS), i)
#define SIMD_FILTER_TYPE(ARITY, WORD_BITS) H4(libfilter_block_simd_, ARITY, x, WORD_BITS)
#define SIMD_METHOD(ARITY, WORD_BITS, x) H3(SIMD_FILTER_TYPE(ARITY, WORD_BITS), _, x)

#define VECOP(ARITY, WORD_BITS, x) \
  H6(_mm, F4(PRODUCT_, ARITY, _, WORD_BITS), _, x, _epi, WORD_BITS)
#define BUCKOP(ARITY, WORD_BITS, x) \
  H6(_mm, F4(PRODUCT_, ARITY, _, WORD_BITS), _, x, _si, F4(PRODUCT_, ARITY, _, WORD_BITS))

// TODO: can probably get rid of all references to WORD_BITS using a combo of
// LOG_WORD_BITS, CHAR_BIT, and memcpy, but may cause a performance regression. OTOH,
// could #define the types uint5 = uint32_t, uint6 = uint64_t. might need VECOP5, VECOP6?

#define DECL_SCALAR(ARITY, WORD_BITS)                                                   \
  typedef struct {                                                                      \
    alignas(BUCKET_BYTES(ARITY, WORD_BITS)) WORD_TYPE(WORD_BITS) payload[ARITY];        \
  } SCALAR_BUCKET_TYPE(ARITY, WORD_BITS);                                               \
                                                                                        \
  static __attribute__((always_inline)) inline SCALAR_BUCKET_TYPE(ARITY, WORD_BITS)     \
      SCALAR_METHOD(ARITY, WORD_BITS, make_mask)(const uint64_t hash) {                 \
    SCALAR_BUCKET_TYPE(ARITY, WORD_BITS) hash_data;                                     \
    const long long seeds[] = {HASH_SEEDS};                                             \
    for (unsigned i = 0; i < BUCKET_BYTES(ARITY, WORD_BITS) / sizeof(long long); ++i) { \
      for (unsigned j = 0; j < sizeof(long long) / sizeof(WORD_TYPE(WORD_BITS)); ++j) { \
        hash_data.payload[sizeof(long long) / sizeof(WORD_TYPE(WORD_BITS)) * i + j] =   \
            ((WORD_TYPE(WORD_BITS))hash) *                                              \
            ((WORD_TYPE(WORD_BITS))(seeds[i] >> (WORD_BITS * j)));                      \
      }                                                                                 \
    }                                                                                   \
    for (unsigned i = 0; i < ARITY; ++i) {                                              \
      hash_data.payload[i] =                                                            \
          (hash_data.payload[i] >> (WORD_BITS - H2(LOG2_, WORD_BITS)));                 \
    }                                                                                   \
    for (unsigned i = 0; i < ARITY; ++i) {                                              \
      hash_data.payload[i] = (((WORD_TYPE(WORD_BITS))1) << hash_data.payload[i]);       \
    }                                                                                   \
    return hash_data;                                                                   \
  }                                                                                     \
                                                                                        \
  static __attribute__((always_inline)) inline void SCALAR_METHOD(ARITY, WORD_BITS,     \
                                                                  add_hash_raw)(        \
      const uint64_t hash, const uint64_t num_buckets,                                  \
      SCALAR_BUCKET_TYPE(ARITY, WORD_BITS)* const block) {                              \
    const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);               \
    const SCALAR_BUCKET_TYPE(ARITY, WORD_BITS) mask =                                   \
        SCALAR_METHOD(ARITY, WORD_BITS, make_mask)(hash);                               \
    SCALAR_BUCKET_TYPE(ARITY, WORD_BITS)* const bucket = block + bucket_idx;            \
    for (unsigned i = 0; i < ARITY; ++i) {                                              \
      bucket->payload[i] = mask.payload[i] | bucket->payload[i];                        \
    }                                                                                   \
  }                                                                                     \
                                                                                        \
  static __attribute__((always_inline)) inline bool SCALAR_METHOD(ARITY, WORD_BITS,     \
                                                                  find_hash_raw)(       \
      const uint64_t hash, const uint64_t num_buckets,                                  \
      const SCALAR_BUCKET_TYPE(ARITY, WORD_BITS)* const block) {                        \
    const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);               \
    const SCALAR_BUCKET_TYPE(ARITY, WORD_BITS) mask =                                   \
        SCALAR_METHOD(ARITY, WORD_BITS, make_mask)(hash);                               \
    const SCALAR_BUCKET_TYPE(ARITY, WORD_BITS) bucket = block[bucket_idx];              \
    /* TODO: branchless? */                                                             \
    for (unsigned i = 0; i < ARITY; ++i) {                                              \
      if (0 == (bucket.payload[i] & mask.payload[i])) return false;                     \
    }                                                                                   \
    return true;                                                                        \
  }                                                                                     \
                                                                                        \
  static __attribute__((always_inline)) inline void SCALAR_METHOD(                      \
      ARITY, WORD_BITS, add_hash)(const uint64_t hash,                                  \
                                  SimdBlockBloomFilter* const here) {                   \
    return SCALAR_METHOD(ARITY, WORD_BITS, add_hash_raw)(                               \
        hash, here->num_buckets_,                                                       \
        (SCALAR_BUCKET_TYPE(ARITY, WORD_BITS)*)here->block_.block);                     \
  }                                                                                     \
                                                                                        \
  static __attribute__((always_inline)) inline bool SCALAR_METHOD(                      \
      ARITY, WORD_BITS, find_hash)(const uint64_t hash,                                 \
                                   const SimdBlockBloomFilter* const here) {            \
    return SCALAR_METHOD(ARITY, WORD_BITS, find_hash_raw)(                              \
        hash, here->num_buckets_,                                                       \
        (const SCALAR_BUCKET_TYPE(ARITY, WORD_BITS)*)here->block_.block);               \
  }                                                                                     \
                                                                                        \
  __attribute__((always_inline)) inline uint64_t SCALAR_METHOD(                         \
      ARITY, WORD_BITS, size_in_bytes)(const SimdBlockBloomFilter* const here) {        \
    return (here->num_buckets_) * (BUCKET_BYTES(ARITY, WORD_BITS));                     \
  }

#define DECL_SCALAR_SHINGLE(ARITY, WORD_BITS, STRIDE)                                    \
  typedef struct {                                                                       \
    alignas(STRIDE) char payload[BUCKET_BYTES(ARITY, WORD_BITS)];                        \
  } SCALAR_SHINGLE_BUCKET_TYPE(ARITY, WORD_BITS, STRIDE);                                \
                                                                                         \
  static __attribute__((always_inline)) inline void SCALAR_SHINGLE_METHOD(               \
      ARITY, WORD_BITS, STRIDE, add_hash_raw)(                                           \
      const uint64_t hash, const uint64_t num_buckets, char* const block) {              \
    const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);                \
    const SCALAR_BUCKET_TYPE(ARITY, WORD_BITS) mask =                                    \
        SCALAR_METHOD(ARITY, WORD_BITS, make_mask)(hash);                                \
    SCALAR_SHINGLE_BUCKET_TYPE(ARITY, WORD_BITS, STRIDE)* const bucket =                 \
        (SCALAR_SHINGLE_BUCKET_TYPE(ARITY, WORD_BITS, STRIDE)*)(block +                  \
                                                                bucket_idx * STRIDE);    \
    for (unsigned i = 0; i < ARITY; ++i) {                                               \
      WORD_TYPE(WORD_BITS) payload_i;                                                    \
      memcpy(&payload_i, &bucket->payload[i * WORD_BITS / CHAR_BIT], sizeof(payload_i)); \
      payload_i = mask.payload[i] | payload_i;                                           \
      memcpy(&bucket->payload[i * WORD_BITS / CHAR_BIT], &payload_i, sizeof(payload_i)); \
    }                                                                                    \
  }                                                                                      \
                                                                                         \
  static __attribute__((always_inline)) inline bool SCALAR_SHINGLE_METHOD(               \
      ARITY, WORD_BITS, STRIDE, find_hash_raw)(                                          \
      const uint64_t hash, const uint64_t num_buckets, const char* const block) {        \
    const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);                \
    const SCALAR_BUCKET_TYPE(ARITY, WORD_BITS) mask =                                    \
        SCALAR_METHOD(ARITY, WORD_BITS, make_mask)(hash);                                \
    const SCALAR_SHINGLE_BUCKET_TYPE(ARITY, WORD_BITS, STRIDE)* bucket =                 \
        (SCALAR_SHINGLE_BUCKET_TYPE(ARITY, WORD_BITS, STRIDE)*)(block +                  \
                                                                bucket_idx * STRIDE);    \
    for (unsigned i = 0; i < ARITY; ++i) {                                               \
      WORD_TYPE(WORD_BITS) payload_i;                                                    \
      memcpy(&payload_i, &bucket->payload[i * WORD_BITS / CHAR_BIT], sizeof(payload_i)); \
      if (0 == (payload_i & mask.payload[i])) return false;                              \
    }                                                                                    \
    return true;                                                                         \
  }                                                                                      \
                                                                                         \
  static __attribute__((always_inline)) inline void SCALAR_SHINGLE_METHOD(               \
      ARITY, WORD_BITS, STRIDE, add_hash)(const uint64_t hash,                           \
                                          SimdBlockBloomFilter* const here) {            \
    return SCALAR_SHINGLE_METHOD(ARITY, WORD_BITS, STRIDE, add_hash_raw)(                \
        hash,                                                                            \
        here->num_buckets_ * ((BUCKET_BYTES(ARITY, WORD_BITS)) / STRIDE) -               \
            ((BUCKET_BYTES(ARITY, WORD_BITS)) / STRIDE - 1),                             \
        (char*)here->block_.block);                                                      \
  }                                                                                      \
                                                                                         \
  static __attribute__((always_inline)) inline bool SCALAR_SHINGLE_METHOD(               \
      ARITY, WORD_BITS, STRIDE, find_hash)(const uint64_t hash,                          \
                                           const SimdBlockBloomFilter* const here) {     \
    return SCALAR_SHINGLE_METHOD(ARITY, WORD_BITS, STRIDE, find_hash_raw)(               \
        hash,                                                                            \
        here->num_buckets_ * ((BUCKET_BYTES(ARITY, WORD_BITS)) / STRIDE) -               \
            ((BUCKET_BYTES(ARITY, WORD_BITS)) / STRIDE - 1),                             \
        (const char*)here->block_.block);                                                \
  }

#define DECL_ALIGNED(ARITY, WORD_BITS)                                                   \
  static __attribute__((always_inline)) inline SIMD_BUCKET_TYPE(ARITY, WORD_BITS)        \
      SIMD_METHOD(ARITY, WORD_BITS, make_mask)(const uint64_t hash) {                    \
    const SIMD_BUCKET_TYPE(ARITY, WORD_BITS) ones = VECOP(ARITY, WORD_BITS, set1)(1);    \
    const SIMD_BUCKET_TYPE(ARITY, WORD_BITS) rehash = {HASH_SEEDS};                      \
    SIMD_BUCKET_TYPE(ARITY, WORD_BITS)                                                   \
    hash_data = VECOP(ARITY, WORD_BITS, set1)(hash);                                     \
    hash_data = VECOP(ARITY, WORD_BITS, mullo)(rehash, hash_data);                       \
    hash_data =                                                                          \
        VECOP(ARITY, WORD_BITS, srli)(hash_data, WORD_BITS - H2(LOG2_, WORD_BITS));      \
    return VECOP(ARITY, WORD_BITS, sllv)(ones, hash_data);                               \
  }                                                                                      \
  static __attribute__((always_inline)) inline void SIMD_METHOD(                         \
      ARITY, WORD_BITS, add_hash_raw)(const uint64_t hash, const uint64_t num_buckets,   \
                                      SIMD_BUCKET_TYPE(ARITY, WORD_BITS)* const block) { \
    const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);                \
    const SIMD_BUCKET_TYPE(ARITY, WORD_BITS) mask =                                      \
        SIMD_METHOD(ARITY, WORD_BITS, make_mask)(hash);                                  \
    SIMD_BUCKET_TYPE(ARITY, WORD_BITS)* const bucket = block + bucket_idx;               \
    BUCKOP(ARITY, WORD_BITS, store)                                                      \
    (bucket, BUCKOP(ARITY, WORD_BITS, or)(*bucket, mask));                               \
  }                                                                                      \
  static __attribute__((always_inline)) inline bool SIMD_METHOD(ARITY, WORD_BITS,        \
                                                                find_hash_raw)(          \
      const uint64_t hash, const uint64_t num_buckets,                                   \
      const SIMD_BUCKET_TYPE(ARITY, WORD_BITS)* const block) {                           \
    const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);                \
    const SIMD_BUCKET_TYPE(ARITY, WORD_BITS) mask =                                      \
        SIMD_METHOD(ARITY, WORD_BITS, make_mask)(hash);                                  \
    const SIMD_BUCKET_TYPE(ARITY, WORD_BITS) bucket = block[bucket_idx];                 \
    return TEST_BUCKET_MASK(bucket, mask);                                               \
  }                                                                                      \
  static __attribute__((always_inline)) inline void SIMD_METHOD(                         \
      ARITY, WORD_BITS, add_hash)(const uint64_t hash,                                   \
                                  SimdBlockBloomFilter* const here) {                    \
    return SIMD_METHOD(ARITY, WORD_BITS, add_hash_raw)(                                  \
        hash, here->num_buckets_,                                                        \
        (SIMD_BUCKET_TYPE(ARITY, WORD_BITS)*)here->block_.block);                        \
  }                                                                                      \
  static __attribute__((always_inline)) inline bool SIMD_METHOD(                         \
      ARITY, WORD_BITS, find_hash)(const uint64_t hash,                                  \
                                   const SimdBlockBloomFilter* const here) {             \
    return SIMD_METHOD(ARITY, WORD_BITS, find_hash_raw)(                                 \
        hash, here->num_buckets_,                                                        \
        (const SIMD_BUCKET_TYPE(ARITY, WORD_BITS)*)here->block_.block);                  \
  }                                                                                      \
  __attribute__((always_inline)) inline uint64_t SIMD_METHOD(                            \
      ARITY, WORD_BITS, size_in_bytes)(const SimdBlockBloomFilter* const here) {         \
    return (here->num_buckets_) * (BUCKET_BYTES(ARITY, WORD_BITS));                      \
  }

#define DECL_SHINGLE(ARITY, WORD_BITS, STRIDE)                                         \
  static __attribute__((always_inline)) inline void SHINGLE_SIMD_METHOD(               \
      ARITY, WORD_BITS, STRIDE, add_hash_raw)(                                         \
      const uint64_t hash, const uint64_t num_buckets, char* const block) {            \
    const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);              \
    const SIMD_BUCKET_TYPE(ARITY, WORD_BITS) mask =                                    \
        SIMD_METHOD(ARITY, WORD_BITS, make_mask)(hash);                                \
    SIMD_BUCKET_TYPE(ARITY, WORD_BITS)* const bucket =                                 \
        (SIMD_BUCKET_TYPE(ARITY, WORD_BITS)*)(block + bucket_idx * STRIDE);            \
    BUCKOP(ARITY, WORD_BITS, storeu)                                                   \
    (bucket,                                                                           \
     BUCKOP(ARITY, WORD_BITS, or)(BUCKOP(ARITY, WORD_BITS, loadu)(bucket), mask));     \
  }                                                                                    \
  static __attribute__((always_inline)) inline bool SHINGLE_SIMD_METHOD(               \
      ARITY, WORD_BITS, STRIDE, find_hash_raw)(                                        \
      const uint64_t hash, const uint64_t num_buckets, const char* const block) {      \
    const uint64_t bucket_idx = libfilter_block_index(hash, num_buckets);              \
    const SIMD_BUCKET_TYPE(ARITY, WORD_BITS) mask =                                    \
        SIMD_METHOD(ARITY, WORD_BITS, make_mask)(hash);                                \
    const SIMD_BUCKET_TYPE(ARITY, WORD_BITS) bucket = BUCKOP(ARITY, WORD_BITS, loadu)( \
        (SIMD_BUCKET_TYPE(ARITY, WORD_BITS)*)(block + bucket_idx * STRIDE));           \
    return TEST_BUCKET_MASK(bucket, mask);                                             \
  }                                                                                    \
  static __attribute__((always_inline)) inline void SHINGLE_SIMD_METHOD(               \
      ARITY, WORD_BITS, STRIDE, add_hash)(const uint64_t hash,                         \
                                          SimdBlockBloomFilter* const here) {          \
    return SHINGLE_SIMD_METHOD(ARITY, WORD_BITS, STRIDE, add_hash_raw)(                \
        hash,                                                                          \
        here->num_buckets_ * ((BUCKET_BYTES(ARITY, WORD_BITS)) / STRIDE) -             \
            ((BUCKET_BYTES(ARITY, WORD_BITS)) / STRIDE - 1),                           \
        (char*)here->block_.block);                                                    \
  }                                                                                    \
  static __attribute__((always_inline)) inline bool SHINGLE_SIMD_METHOD(               \
      ARITY, WORD_BITS, STRIDE, find_hash)(const uint64_t hash,                        \
                                           const SimdBlockBloomFilter* const here) {   \
    return SHINGLE_SIMD_METHOD(ARITY, WORD_BITS, STRIDE, find_hash_raw)(               \
        hash,                                                                          \
        here->num_buckets_ * ((BUCKET_BYTES(ARITY, WORD_BITS)) / STRIDE) -             \
            ((BUCKET_BYTES(ARITY, WORD_BITS)) / STRIDE - 1),                           \
        (const char*)here->block_.block);                                              \
  }

#define SIMD_DEFAULT_DECL(ARITY, WORD_BITS)                                  \
  static __attribute__((always_inline)) inline void DEFAULT_METHOD(          \
      ARITY, WORD_BITS, add_hash)(const uint64_t hash,                       \
                                  SimdBlockBloomFilter* const here) {        \
    return SIMD_METHOD(ARITY, WORD_BITS, add_hash)(hash, here);              \
  }                                                                          \
                                                                             \
  static __attribute__((always_inline)) inline bool DEFAULT_METHOD(          \
      ARITY, WORD_BITS, find_hash)(const uint64_t hash,                      \
                                   const SimdBlockBloomFilter* const here) { \
    return SIMD_METHOD(ARITY, WORD_BITS, find_hash)(hash, here);             \
  }

#define SHINGLE_SIMD_DEFAULT_DECL(ARITY, WORD_BITS, STRIDE)                          \
  static __attribute__((always_inline)) inline void DEFAULT_SHINGLE_METHOD(          \
      ARITY, WORD_BITS, STRIDE, add_hash)(const uint64_t hash,                       \
                                          SimdBlockBloomFilter* const here) {        \
    return SHINGLE_SIMD_METHOD(ARITY, WORD_BITS, STRIDE, add_hash)(hash, here);      \
  }                                                                                  \
                                                                                     \
  static __attribute__((always_inline)) inline bool DEFAULT_SHINGLE_METHOD(          \
      ARITY, WORD_BITS, STRIDE, find_hash)(const uint64_t hash,                      \
                                           const SimdBlockBloomFilter* const here) { \
    return SHINGLE_SIMD_METHOD(ARITY, WORD_BITS, STRIDE, find_hash)(hash, here);     \
  }

#define SCALAR_DEFAULT_DECL(ARITY, WORD_BITS)                                \
  static __attribute__((always_inline)) inline void DEFAULT_METHOD(          \
      ARITY, WORD_BITS, add_hash)(const uint64_t hash,                       \
                                  SimdBlockBloomFilter* const here) {        \
    return SCALAR_METHOD(ARITY, WORD_BITS, add_hash)(hash, here);            \
  }                                                                          \
                                                                             \
  static __attribute__((always_inline)) inline bool DEFAULT_METHOD(          \
      ARITY, WORD_BITS, find_hash)(const uint64_t hash,                      \
                                   const SimdBlockBloomFilter* const here) { \
    return SCALAR_METHOD(ARITY, WORD_BITS, find_hash)(hash, here);           \
  }

#define SHINGLE_SCALAR_DEFAULT_DECL(ARITY, WORD_BITS, STRIDE)                        \
  static __attribute__((always_inline)) inline void DEFAULT_SHINGLE_METHOD(          \
      ARITY, WORD_BITS, STRIDE, add_hash)(const uint64_t hash,                       \
                                          SimdBlockBloomFilter* const here) {        \
    return SCALAR_SHINGLE_METHOD(ARITY, WORD_BITS, STRIDE, add_hash)(hash, here);    \
  }                                                                                  \
                                                                                     \
  static __attribute__((always_inline)) inline bool DEFAULT_SHINGLE_METHOD(          \
      ARITY, WORD_BITS, STRIDE, find_hash)(const uint64_t hash,                      \
                                           const SimdBlockBloomFilter* const here) { \
    return SCALAR_SHINGLE_METHOD(ARITY, WORD_BITS, STRIDE, find_hash)(hash, here);   \
  }

#define DECL_SCALAR_BOTH(ARITY, WORD_BITS) \
  DECL_SCALAR(ARITY, WORD_BITS)            \
  DECL_SCALAR_SHINGLE(ARITY, WORD_BITS, 1) \
  DECL_SCALAR_SHINGLE(ARITY, WORD_BITS, 2) \
  DECL_SCALAR_SHINGLE(ARITY, WORD_BITS, 4) \
  DECL_SCALAR_SHINGLE(ARITY, WORD_BITS, 8) \
  DECL_SCALAR_SHINGLE(ARITY, WORD_BITS, 16)

#define DECL_SIMD_BOTH(ARITY, WORD_BITS) \
  DECL_ALIGNED(ARITY, WORD_BITS)         \
  DECL_SHINGLE(ARITY, WORD_BITS, 1)      \
  DECL_SHINGLE(ARITY, WORD_BITS, 2)      \
  DECL_SHINGLE(ARITY, WORD_BITS, 4)      \
  DECL_SHINGLE(ARITY, WORD_BITS, 8)      \
  DECL_SHINGLE(ARITY, WORD_BITS, 16)

#define SIMD_DEFAULT_BOTH_DECL(ARITY, WORD_BITS) \
  SIMD_DEFAULT_DECL(ARITY, WORD_BITS)            \
  SHINGLE_SIMD_DEFAULT_DECL(ARITY, WORD_BITS, 1) \
  SHINGLE_SIMD_DEFAULT_DECL(ARITY, WORD_BITS, 2) \
  SHINGLE_SIMD_DEFAULT_DECL(ARITY, WORD_BITS, 4) \
  SHINGLE_SIMD_DEFAULT_DECL(ARITY, WORD_BITS, 8) \
  SHINGLE_SIMD_DEFAULT_DECL(ARITY, WORD_BITS, 16)

#define SCALAR_DEFAULT_BOTH_DECL(ARITY, WORD_BITS) \
  SCALAR_DEFAULT_DECL(ARITY, WORD_BITS)            \
  SHINGLE_SCALAR_DEFAULT_DECL(ARITY, WORD_BITS, 1) \
  SHINGLE_SCALAR_DEFAULT_DECL(ARITY, WORD_BITS, 2) \
  SHINGLE_SCALAR_DEFAULT_DECL(ARITY, WORD_BITS, 4) \
  SHINGLE_SCALAR_DEFAULT_DECL(ARITY, WORD_BITS, 8) \
  SHINGLE_SCALAR_DEFAULT_DECL(ARITY, WORD_BITS, 16)

// TODO: allow user-specified hash seeds

#define HASH_SEEDS                                              \
  (long long)0x47b6137b44974d91, (long long)0x8824ad5ba2b7289d, \
      (long long)0x705495c72df1424b, (long long)0x9efc49475c6bfb31
#define TEST_BUCKET_MASK(bucket, mask) _mm256_testc_si256(bucket, mask);

// TODO: test 0xff = _mm256_test_epi32_mask for performance
DECL_SCALAR_BOTH(8, 32)
DECL_SCALAR_SHINGLE(8, 32, 32)
#define libfilter_block_8x32_size_in_bytes libfilter_block_scalar_8x32_size_in_bytes
#define libfilter_block_32_size_in_bytes libfilter_block_scalar_8x32_size_in_bytes

#if defined(__AVX2__)
#define LIBFILTER_SIMD_5_256
DECL_SIMD_BOTH(8, 32)
DECL_SHINGLE(8, 32, 32)
SIMD_DEFAULT_BOTH_DECL(8, 32)
SHINGLE_SIMD_DEFAULT_DECL(8, 32, 32)
#else
SCALAR_DEFAULT_BOTH_DECL(8, 32)
SHINGLE_SCALAR_DEFAULT_DECL(8, 32, 32)
#endif
#undef HASH_SEEDS
#undef TEST_BUCKET_MASK

// TODO: very fine-grained includes to use the SIMD instructions available even when not
// ALL are available.

#define HASH_SEEDS (long long)0x47b6137b44974d91, (long long)0x8824ad5ba2b7289d
#define TEST_BUCKET_MASK(bucket, mask) _mm_testc_si128(bucket, mask);

DECL_SCALAR_BOTH(4, 32)
#define libfilter_block_4x32_size_in_bytes libfilter_block_scalar_4x32_size_in_bytes
#define libfilter_block_16_size_in_bytes libfilter_block_scalar_4x32_size_in_bytes

#if defined(__SSE2__) && defined(__SSE4_1__) && defined(__AVX2__)
#define LIBFILTER_SIMD_5_128
// SSE2
__m128i _mm128_set1_epi32(int a) { return _mm_set1_epi32(a); }
void _mm128_store_si128(__m128i* mem_addr, __m128i a) {
  return _mm_store_si128(mem_addr, a);
}
__m128i _mm128_or_si128(__m128i a, __m128i b) { return _mm_or_si128(a, b); }
// TODO: just do sllv in scalar, the rest in SIMD?
// AVX2
__m128i _mm128_sllv_epi32(__m128i a, __m128i count) { return _mm_sllv_epi32(a, count); }
__m128i _mm128_srli_epi32(__m128i a, int imm8) { return _mm_srli_epi32(a, imm8); }
#define _mm128_loadu_si128 _mm_loadu_si128
#define _mm128_storeu_si128 _mm_storeu_si128
// SSE4.1
__m128i _mm128_mullo_epi32(__m128i a, __m128i b) { return _mm_mullo_epi32(a, b); }
DECL_SIMD_BOTH(4, 32)
SIMD_DEFAULT_BOTH_DECL(4, 32)
#else
SCALAR_DEFAULT_BOTH_DECL(4, 32)
#endif

#undef HASH_SEEDS
#undef TEST_BUCKET_MASK

#define HASH_SEEDS                                                  \
  (long long)0xbde092b5c96d5933, (long long)0x08b89d2194dfa1c7,     \
      (long long)0xaba18283cabfa9af, (long long)0xbfead9ffe14974ff, \
      (long long)0x54c873a787e43e49, (long long)0x153b10ebe53e58cd, \
      (long long)0xe6c4f969b94b97bd, (long long)0x520a6bd58b9b1117
#define TEST_BUCKET_MASK(bucket, mask) 0xffff == _mm512_test_epi32_mask(bucket, mask)

DECL_SCALAR_BOTH(16, 32)
DECL_SCALAR_SHINGLE(16, 32, 32)
DECL_SCALAR_SHINGLE(16, 32, 64)
#define libfilter_block_16x32_size_in_bytes libfilter_block_scalar_16x32_size_in_bytes
#define libfilter_block_64_size_in_bytes libfilter_block_scalar_16x32_size_in_bytes

#if defined(__AVX512F__)
#define LIBFILTER_SIMD_5_512
DECL_SIMD_BOTH(16, 32)
SIMD_DEFAULT_BOTH_DECL(16, 32)
DECL_SHINGLE(16, 32, 32)
DECL_SHINGLE(16, 32, 64)
SHINGLE_SIMD_DEFAULT_DECL(16, 32, 32)
SHINGLE_SIMD_DEFAULT_DECL(16, 32, 64)
#else
SCALAR_DEFAULT_BOTH_DECL(16, 32)
SHINGLE_SCALAR_DEFAULT_DECL(16, 32, 32)
SHINGLE_SCALAR_DEFAULT_DECL(16, 32, 64)
#endif

#undef HASH_SEEDS
#undef TEST_BUCKET_MASK

#define HASH_SEEDS                                                  \
  (long long)0x47b7137b44974d91, (long long)0x8825ad5ba2b7289d,     \
      (long long)0x705595c72df1424b, (long long)0x9efd49475c6bfb31, \
      (long long)0x09eca8b01577456b, (long long)0x83ecf76d31d9d409, \
      (long long)0x0daab5dc5e814f73, (long long)0xbc3107cbb9d8355
#define TEST_BUCKET_MASK(bucket, mask) 0xffu == _mm512_test_epi64_mask(bucket, mask)

DECL_SCALAR_BOTH(8, 64)
DECL_SCALAR_SHINGLE(8, 64, 32)
DECL_SCALAR_SHINGLE(8, 64, 64)
#define libfilter_block_8x64_size_in_bytes libfilter_block_scalar_8x64_size_in_bytes

#if defined(__AVX512F__) && defined(__AVX512DQ__)
#define LIBFILTER_SIMD_6_512
DECL_SIMD_BOTH(8, 64)
SIMD_DEFAULT_BOTH_DECL(8, 64)
DECL_SHINGLE(8, 64, 32)
DECL_SHINGLE(8, 64, 64)
SHINGLE_SIMD_DEFAULT_DECL(8, 64, 32)
SHINGLE_SIMD_DEFAULT_DECL(8, 64, 64)
#else
SCALAR_DEFAULT_BOTH_DECL(8, 64)
SHINGLE_SCALAR_DEFAULT_DECL(8, 64, 32)
SHINGLE_SCALAR_DEFAULT_DECL(8, 64, 64)
#endif

#undef HASH_SEEDS
#undef TEST_BUCKET_MASK

#define HASH_SEEDS                                              \
  (long long)0xe9bdff76ee6d2471, (long long)0x0dece05e9d14fa03, \
      (long long)0xd6d062f51a1a509f, (long long)0xcd28767202575b43,
#define TEST_BUCKET_MASK(bucket, mask) _mm256_testc_si256(bucket, mask)
#define _mm256_set1_epi64 _mm256_set1_epi64x

DECL_SCALAR_BOTH(4, 64)
DECL_SCALAR_SHINGLE(4, 64, 32)
#define libfilter_block_4x64_size_in_bytes libfilter_block_scalar_4x64_size_in_bytes

#if defined(__AVX512F__) && defined(__AVX512DQ__) && defined(__AVX512VL__)
#define LIBFILTER_SIMD_6_256
DECL_SIMD_BOTH(4, 64)
SIMD_DEFAULT_BOTH_DECL(4, 64)
DECL_SHINGLE(4, 64, 32)
SHINGLE_SIMD_DEFAULT_DECL(4, 64, 32)
#else
SCALAR_DEFAULT_BOTH_DECL(4, 64)
SHINGLE_SCALAR_DEFAULT_DECL(4, 64, 32)
#endif

#undef HASH_SEEDS
#undef TEST_BUCKET_MASK

#pragma pop_macro("LOG_WORD_BITS")
#pragma pop_macro("BUCKET_BITS")
#pragma pop_macro("WORD_BITS")
#pragma pop_macro("HASH_SEEDS")
#pragma pop_macro("TEST_BUCKET_MASK")
