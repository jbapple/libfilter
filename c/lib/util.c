#include <limits.h>             // for CHAR_BIT
#include <math.h>               // for log, exp, lgamma, pow
#include <stdint.h>             // for uint64_t

double libfilter_block_fpp_detail(double ndv, double bytes, double word_bits,
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

uint64_t libfilter_block_bytes_needed_detail(double ndv, double fpp, double word_bits,
                                             double bucket_words, double hash_bits) {
  const uint64_t bucket_bytes = (word_bits * bucket_words) / CHAR_BIT;
  uint64_t result = 1;
  while (libfilter_block_fpp_detail(ndv, result, word_bits, bucket_words, hash_bits) > fpp) {
    if (result * 2 < result) return result;
    result *= 2;
  }
  if (result <= bucket_bytes) return bucket_bytes;
  uint64_t lo = 0;
  while (lo + 1 < result) {
    // fprintf(stderr, "%d %d\n", (int)lo, (int)result);
    const uint64_t mid = lo + (result - lo) / 2;
    const double test = libfilter_block_fpp_detail(ndv, mid, word_bits, bucket_words, hash_bits);
    if (test < fpp)
      result = mid;
    else if (test == fpp)
      return ((mid + bucket_bytes - 1) / bucket_bytes) * bucket_bytes;
    else
      lo = mid;
  }
  return ((result + bucket_bytes - 1) / bucket_bytes) * bucket_bytes;
}

uint64_t libfilter_block_capacity_detail(uint64_t bytes, double fpp, double word_bits,
                                         double bucket_words, double hash_bits) {
  uint64_t result = 1;
  // TODO: unify this exponential + binary search with the bytes needed function above
  while (libfilter_block_fpp_detail(result, bytes, word_bits, bucket_words, hash_bits) < fpp) {
    result *= 2;
  }
  if (result == 1) return 0;
  uint64_t lo = 0;
  while (lo + 1 < result) {
    const uint64_t mid = lo + (result - lo) / 2;
    const double test =
      libfilter_block_fpp_detail(mid, bytes, word_bits, bucket_words, hash_bits);
    if (test < fpp)
      lo = mid;
    else if (test == fpp)
      return mid;
    else
      result = mid;
  }
  return lo;
}
