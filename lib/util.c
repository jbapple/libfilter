#include "util.h"

#include "memory.h"
#include "pcg_variants.h"

double libfilter_block_fpp(double ndv, double bytes, double word_bits,
                           double bucket_words, double hash_bits) {
  // From equation 3 of Putze et al.
  //
  // m = bytes * CHAR_BIT
  // n = ndv
  // c = m / n
  // b = m / (bucket_words * word_bits)
  // B = n * c / b = n * (m / n) / (m / (bucket_words * word_bits)) = m / (m /
  // (bucket_words * word_bits)) = bucket_words * word_bits k = number of hashes, in our
  // case bucket_words
  if (ndv == 0) return 0.0;
  if (bytes <= 0) return 1.0;
  if (ndv / (bytes * CHAR_BIT) > 3) return 1.0;

  double result = 0;
  const double lam =
      bucket_words * (double)word_bits / (((double)bytes * CHAR_BIT) / (double)ndv);
  const double loglam = log(lam);
  const double log1collide = -hash_bits * log(2.0);
  const uint64_t MAX_J = 10000;
  for (uint64_t j = 0; j < MAX_J; ++j) {
    uint64_t i = MAX_J - 1 - j;
    double logp = i * loglam - lam - lgamma(i + 1);
    const double logfinner = bucket_words * log(1.0 - pow(1.0 - 1.0 / word_bits, i));
    const double logcollide = log(i) + log1collide;
    result += exp(logp + logfinner) + exp(logp + logcollide);
  }
  return (result > 1.0) ? 1.0 : result;
}

double libfilter_block_shingle_1_fpp(double ndv, double bytes, double word_bits,
                                     double bucket_words, double hash_bits) {
  (void)hash_bits;
  if (ndv == 0) return 0.0;
  if (bytes <= 0) return 1.0;
  if (ndv / (bytes * CHAR_BIT) > 3) return 1.0;

  uint64_t SAMPLE_SIZE = 10000;
  uint64_t* fills = (uint64_t*)calloc(2 * bucket_words - 1, sizeof(uint64_t));
  const double lambda = ndv / (bytes / (word_bits / CHAR_BIT) - bucket_words + 1);
  const double big_l = exp(-lambda);
  pcg64s_random_t r;
  FILE* rand_bytes = fopen("/dev/urandom", "r");
  assert(NULL != rand_bytes);
  uint64_t seed_tmp;
  size_t progress = fread(&seed_tmp, 1, sizeof(seed_tmp), rand_bytes);
  assert(sizeof(seed_tmp) == progress);
  pcg128_t seed = seed_tmp;
  seed = seed << 64;
  progress = fread(&seed_tmp, 1, sizeof(seed_tmp), rand_bytes);
  assert(sizeof(seed_tmp) == progress);
  seed |= seed_tmp;
  pcg64s_srandom_r(&r, seed);

  double sum = 0.0;
  for (uint64_t i = 0; i < SAMPLE_SIZE; ++i) {
    for (int j = 0; j < 2 * bucket_words - 1; ++j) {
      // Knuth's method for sampling from a Poisson distribution
      uint64_t k = 0;
      double p = 1;
      do {
        ++k;
        uint64_t random_bits = pcg64s_random_r(&r);
        p = p * (random_bits >> 32) / (ldexp(1.0, 32) - 1);
        if (p <= big_l) break;
        ++k;
        p = p * ((uint32_t)random_bits) / (ldexp(1.0, 32) - 1);
      } while(p > big_l);
      --k;
      fills[j] = k;
      // end Knuth's method

      // begin method without many random number generations
      // uint64_t x = 0;
      // double p = big_l;
      // double s = p;
      // double u = ((double)pcg64s_random_r(&r)) / (ldexp(1.0, 64) - 1);
      // while (u > s) {
      //   ++x;
      //   p*= lambda / x;
      //   s += p;
      // }
      // fills[j] = x;
      // end low-rng method
    }
    double local_product = 1.0;
    for (int j = 0; j < bucket_words; ++j) {
      int count = 0;
      for (int n = 0; n < bucket_words; ++n) {
        count += fills[j + n];
      }
      // TODO: do this in logspace?
      local_product *= (1 - pow(1 - (1 / word_bits), count));
    }
    sum += local_product;
    if (0 == (i & (i-1))) {
      // printf("\t%f\t%lu\n", sum / (i + 1), i + 1);
    }
  }
  return sum / SAMPLE_SIZE;
}

uint64_t libfilter_block_bytes_needed(double ndv, double fpp, double word_bits,
                                      double bucket_words, double hash_bits) {
  const uint64_t bucket_bytes = (word_bits * bucket_words) / CHAR_BIT;
  uint64_t result = 1;
  while (libfilter_block_fpp(ndv, result, word_bits, bucket_words, hash_bits) > fpp) {
    result *= 2;
  }
  if (result <= bucket_bytes) return bucket_bytes;
  uint64_t lo = 0;
  while (lo + 1 < result) {
    // fprintf(stderr, "%d %d\n", (int)lo, (int)result);
    const uint64_t mid = lo + (result - lo) / 2;
    const double test = libfilter_block_fpp(ndv, mid, word_bits, bucket_words, hash_bits);
    if (test < fpp)
      result = mid;
    else if (test == fpp)
      return ((mid + bucket_bytes - 1) / bucket_bytes) * bucket_bytes;
    else
      lo = mid;
  }
  return ((result + bucket_bytes - 1) / bucket_bytes) * bucket_bytes;
}

uint64_t libfilter_generic_bytes_needed(double ndv, double fpp, double word_bits,
                                        double bucket_words, double hash_bits,
                                        double (*calc)(double, double, double, double,
                                                       double)) {
  const uint64_t bucket_bytes = (word_bits * bucket_words) / CHAR_BIT;
  uint64_t result = 1;
  while (calc(ndv, result, word_bits, bucket_words, hash_bits) > fpp) {
    result *= 2;
  }
  if (result <= bucket_bytes) return bucket_bytes;
  uint64_t lo = 0;
  while (lo + 1 < result) {
    // fprintf(stderr, "%d %d\n", (int)lo, (int)result);
    const uint64_t mid = lo + (result - lo) / 2;
    const double test = calc(ndv, mid, word_bits, bucket_words, hash_bits);
    if (test < fpp)
      result = mid;
    else if (test == fpp)
      return ((mid + bucket_bytes - 1) / bucket_bytes) * bucket_bytes;
    else
      lo = mid;
  }
  return ((result + bucket_bytes - 1) / bucket_bytes) * bucket_bytes;
}

uint64_t libfilter_block_capacity(uint64_t bytes, double fpp, double word_bits,
                                  double bucket_words, double hash_bits) {
  uint64_t result = 1;
  while (libfilter_block_fpp(result, bytes, word_bits, bucket_words, hash_bits) < fpp) {
    result *= 2;
  }
  if (result == 1) return 0;
  uint64_t lo = 0;
  while (lo + 1 < result) {
    const uint64_t mid = lo + (result - lo) / 2;
    const double test =
        libfilter_block_fpp(mid, bytes, word_bits, bucket_words, hash_bits);
    if (test < fpp)
      lo = mid;
    else if (test == fpp)
      return mid;
    else
      result = mid;
  }
  return lo;
}

int libfilter_block_init(const uint64_t heap_space, const uint64_t bucket_bytes,
                         SimdBlockBloomFilter* const here) {
  const libfilter_region_alloc_result allocated =
      libfilter_do_alloc(heap_space, bucket_bytes);
  if (0 == allocated.block_bytes) return -1;
  if (!allocated.zero_filled) memset(allocated.region.block, 0, allocated.block_bytes);
  here->num_buckets_ = allocated.block_bytes / bucket_bytes;
  here->block_ = allocated.region;
  return 0;
}

// TODO: intersection, union, serialize, deserialize

bool libfilter_block_destruct(const uint64_t bucket_bytes,
                              SimdBlockBloomFilter* const here) {
  const bool result =
      libfilter_do_free(here->block_, here->num_buckets_ * bucket_bytes, bucket_bytes);
  here->num_buckets_ = 0;
  libfilter_clear_region(&here->block_);
  return result;
}
