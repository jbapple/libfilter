#include "simd-block.h"
// #include <libfilter/simd-block.h>

#include <assert.h>
#include <limits.h>

int main() {
  const unsigned word_bits = 32;
  const unsigned bucket_bits = 256;
  const unsigned ndv = 1000000;
  const double fpp = 0.05;
  const unsigned bytes = libfilter_block_bytes_needed(ndv, fpp, word_bits,
                                                      bucket_bits / word_bits, word_bits);
  SimdBlockBloomFilter filter;
  libfilter_block_init(bytes, bucket_bits / CHAR_BIT, &filter);
  // 5_256 because word_bits = (1 << 5) and bucket_bits = 256
  //
  // TODO: do we have enough randomness when hash is 64 bits? we use some to pick the
  // bucket, leaving fewer than 64
  assert(libfilter_block_8x32_size_in_bytes(&filter) <= bytes);
  const uint64_t hash = 0xfeedbadbee52b055;
  libfilter_block_8x32_1_add_hash(hash, &filter);
  assert(libfilter_block_8x32_1_find_hash(hash, &filter));
  libfilter_block_destruct(bucket_bits / CHAR_BIT, &filter);
}
