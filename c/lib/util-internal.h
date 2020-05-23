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

#include <stdbool.h>  // for bool
#include <stdint.h>   // for uint64_t

#include "memory.h"   // for libfilter_region

// Produces the false positive probability for a filter with `ndv` distinct values,
// `bytes` space available for use (pretending the metadata space required is zero),
// `word_bits` for the size of each lane in a bucket, `bucket_words` lanes per bucket, and
// where each value is hashed (before inserting) to `hash_bits` bits. This late parameter
// has little effect on the result; it is mainly present to account for "hard" collisions,
// wehre two distinct values are indistinguishable to any hash-based sketch structure
// because they have the same hash value.
double libfilter_block_fpp_detail(double ndv, double bytes, double word_bits,
                                  double bucket_words, double hash_bits);

// Returns the amount of space needed to support the given number of distinct values wih
// the given false positive probability.
uint64_t libfilter_block_bytes_needed_detail(double ndv, double fpp, double word_bits,
                                             double bucket_words, double hash_bits);

// Returns the number of distinct values that can be inserted into a filter of size
// `bytes` without exceeding false positive probability `fpp`.
uint64_t libfilter_block_capacity_detail(uint64_t bytes, double fpp, double word_bits,
                                         double bucket_words, double hash_bits);
