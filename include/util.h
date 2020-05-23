#pragma once

// This produces the model false positive probability for a split block Bloom filter.
//
// ndv: number of distinct values
// bytes: the size of the filter in bytes
// word_bits: the number of bits in each word of the block
// bucket_words: the number of words in each block
// hash_bits: the number of bits in the haah value. Unlikely to induce any extra false
// positive probability unless less than 32.
//
// @inproceedings{putze2007cache,
//   title={Cache-, hash-and space-efficient bloom filters},
//   author={Putze, Felix and Sanders, Peter and Singler, Johannes},
//   booktitle={International Workshop on Experimental and Efficient Algorithms},
//   pages={108--121},
//   year={2007},
//   organization={Springer}
// }

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>  // for memset

#include "memory.h"

double libfilter_block_fpp(double ndv, double bytes, double word_bits,
                           double bucket_words, double hash_bits);

double libfilter_block_shingle_1_fpp(double ndv, double bytes, double word_bits,
                                     double bucket_words, double hash_bits);

uint64_t libfilter_block_bytes_needed(double ndv, double fpp, double word_bits,
                                      double bucket_words, double hash_bits);

uint64_t libfilter_generic_bytes_needed(double ndv, double fpp, double word_bits,
                                        double bucket_words, double hash_bits,
                                        double (*calc)(double, double, double, double,
                                                       double));

uint64_t libfilter_block_capacity(uint64_t bytes, double fpp, double word_bits,
                                  double bucket_words, double hash_bits);

typedef struct {
  uint64_t num_buckets_;
  libfilter_region block_;
} SimdBlockBloomFilter;

int libfilter_block_init(const uint64_t heap_space, const uint64_t bucket_bytes,
                         SimdBlockBloomFilter* const here);

bool libfilter_block_destruct(const uint64_t bucket_bytes,
                              SimdBlockBloomFilter* const here);

__attribute__((always_inline)) inline uint64_t libfilter_block_index(
    const uint64_t hash, const uint64_t num_buckets) {
  return (((unsigned __int128)hash) * ((unsigned __int128)num_buckets)) >> 64;
}

// TODO: intersection, union, serialize, deserialize
