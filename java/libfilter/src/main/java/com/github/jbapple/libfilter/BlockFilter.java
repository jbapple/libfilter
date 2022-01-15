package com.github.jbapple.libfilter;

import org.apache.commons.math3.special.Gamma;

import java.lang.Math;
import java.lang.NullPointerException;
import java.util.Arrays;

/**
 * BlockFilter is a block Bloom filter (from Putze et al.'s "Cache-, Hash- and
 * Space-Efficient Bloom Filters") with some twists:
 * <ol>
 * <li> Each block is a split Bloom filter - see Section 2.1 of Broder and Mitzenmacher's
 *      "Network Applications of Bloom Filters: A Survey".
 * <li> The number of bits set during each key addition is constant in order to try and
 *      take advantage of SIMD instructions.
 * </ol>
 */
public class BlockFilter implements Comparable<BlockFilter>, Cloneable, Filter {
  @Override
  public int compareTo(BlockFilter o) {
    // TODO: this is not lexicographic
    if (o == null) throw new NullPointerException();
    if (payload.length < o.payload.length) return -1;
    if (payload.length > o.payload.length) return +1;
    for (int i = 0; i < payload.length; ++i) {
      if (payload[i] < o.payload[i]) return -1;
      if (payload[i] > o.payload[i]) return +1;
    }
    return 0;
  }

  /**
   * Calculates the expected false positive probability from the size (in bytes) and fill
   * factor (in number of distinct values, <code>ndv</code>).
   *
   * @param ndv   the number of distinct values
   * @param bytes the size of the filter
   * @return the false positive probability
   */
  public static double Fpp(double ndv, double bytes) {
    double word_bits = 32;
    double bucket_words = 8;
    double hash_bits = 32;
    // From equation 3 of Putze et al.
    //
    // m = bytes * CHAR_BIT
    // n = ndv
    // c = m / n
    // b = m / (bucket_words * word_bits)
    // B = n * c / b = n * (m / n) / (m / (bucket_words * word_bits)) = m / (m /
    // (bucket_words * word_bits)) = bucket_words * word_bits k = number of hashes, in our
    // case: bucket_words
    if (ndv == 0) return 0.0;
    if (bytes <= 0) return 1.0;
    if (ndv / (1.0 * bytes * 8) > 3) return 1.0;

    double result = 0;
    double lam =
        bucket_words * word_bits / (( bytes * 8) / ndv);
    double loglam = Math.log(lam);
    double log1collide = -hash_bits * Math.log(2.0);
    int MAX_J = 10000;
    for (int j = 0; j < MAX_J; ++j) {
      int i = MAX_J - 1 - j;
      double logp = i * loglam - lam - Gamma.logGamma(i + 1);
      double logfinner =
          bucket_words * Math.log(1.0 - Math.pow(1.0 - 1.0 / word_bits, i));
      double logcollide = Math.log(i) + log1collide;
      result += Math.exp(logp + logfinner) + Math.exp(logp + logcollide);
    }
    return Math.min(result, 1.0);
  }

  /**
   * Calculates the number of bytes needed to create a filter with the given number of
   * distinct values and  false positive probability.
   *
   * @param ndv  the number of distinct values
   * @param fpp  the desired false positive probability
   * @return the number of bytes needed
   */
  public static int BytesNeeded(double ndv, double fpp) {
    double word_bits = 32;
    double bucket_words = 8;
    int bucket_bytes = (int) ((word_bits * bucket_words) / 8);
    double result = 1;
    while (Fpp(ndv, result) > fpp) {
      result *= 2;
    }
    if (result <= bucket_bytes) return bucket_bytes;
    double lo = 0;
    while (lo + 1 < result) {
      double mid = lo + (result - lo) / 2;
      double test = Fpp(ndv, mid);
      if (test < fpp) {
        result = mid;
      } else if (test == fpp) {
        result = ((mid + bucket_bytes - 1) / bucket_bytes) * bucket_bytes;
        if (result > Integer.MAX_VALUE) return Integer.MAX_VALUE;
        return (int) result;
      } else {
        lo = mid;
      }
    }
    result = ((result + bucket_bytes - 1) / bucket_bytes) * bucket_bytes;
    if (result > Integer.MAX_VALUE) return Integer.MAX_VALUE;
    return (int)result;
  }

  /**
   * Calculates the number of distinct elements that can be stored in a filter of size
   * <code>bytes</code> without exceeding the given false positive probability.
   *
   * @param bytes the size of the filter
   * @param fpp   the desired false positive probability
   * @return the maximum number of distinct values that can be added
   */
  public static double TotalHashCapacity(double bytes, double fpp) {
    double result = 1;
    // TODO: unify this exponential + binary search with the bytes needed function above
    while (Fpp(result, bytes) < fpp) {
      result *= 2;
    }
    if (result == 1) return 0;
    double lo = 0;
    while (lo + 1 < result) {
       double mid = lo + (result - lo) / 2;
       double test = Fpp(mid, bytes);
       if (test < fpp)
         lo = mid;
       else if (test == fpp)
         return mid;
       else
         result = mid;
    }
    return lo;
  }

  @Override
  public BlockFilter clone() {
    BlockFilter result = new BlockFilter(0);
    result.payload = payload.clone();
    return result;
  }

  @Override
  public boolean equals(Object there) {
    if (there == null) return false;
    if (!(there instanceof BlockFilter)) return false;
    BlockFilter that = (BlockFilter) there;
    return Arrays.equals(payload, that.payload);
  }

  @Override
  public int hashCode() {
    return Arrays.hashCode(payload);
  }

  @Override
  public String toString() {
    return Arrays.toString(payload);
  }

  private int[] payload;

  public long sizeInBytes() { return payload.length * 4L; }

  BlockFilter(int bytes) { payload = new int[Math.max(8, (bytes / 4) / 8 * 8)]; }

  /**
   * Create a new filter of a given size.
   * <p>
   * The result might be slightly smaller (by up to 31 bytes).
   *
   * @param bytes the size of the filter
   */
  public static BlockFilter CreateWithBytes(int bytes) { return new BlockFilter(bytes); }

  /**
   * Create a new filter to hold the given number of distinct values with a false positive
   * probability of no more than <code>fpp</code>.
   *
   * @param ndv the number of distinct values
   * @param fpp the false positive probability
   */
  public static BlockFilter CreateWithNdvFpp(double ndv, double fpp) {
    int bytes = BytesNeeded(ndv, fpp);
    return new BlockFilter(bytes);
  }

  private static final int[] INTERNAL_HASH_SEEDS = {0x47b6137b, 0x44974d91, 0x8824ad5b,
      0xa2b7289d, 0x705495c7, 0x2df1424b, 0x9efc4947, 0x5c6bfb31};

  private int Index(long hash, int num_buckets) {
    return (int)(((hash >>> 32) * ((long) num_buckets)) >>> 32);
  }

  private void MakeMask(long hash, int[] result) {
    // The loops are done individually for easier auto-vectorization by the JIT
    for (int i = 0; i < 8; ++i) {
      result[0] = (int) hash * INTERNAL_HASH_SEEDS[i];
    }
    for (int i = 0; i < 8; ++i) {
      result[i] = result[i] >>> (32 - 5);
    }
    for (int i = 0; i < 8; ++i) {
      result[i] = 1 << result[i];
    }
  }

  @Override
  public boolean AddHash64(long hash) {
    int bucket_idx = Index(hash, payload.length / 8);
    int[] mask = {0xca11, 8, 6, 7, 5, 3, 0, 9};
    MakeMask(hash, mask);
    for (int i = 0; i < 8; ++i) {
      payload[bucket_idx * 8 + i] = mask[i] | payload[bucket_idx * 8 + i];
    }
    return true;
  }

  @Override
  public boolean FindHash64(long hash) {
    int bucket_idx = Index(hash, payload.length / 8);
    int[] mask = {0xca11, 8, 6, 7, 5, 3, 0, 9};
    MakeMask(hash, mask);
    for (int i = 0; i < 8; ++i) {
      if (0 == (payload[bucket_idx * 8 + i] & mask[i])) return false;
    }
    return true;
  }

  private static final long REHASH_32 = (((long) 0xd1012a3a) << 32) | (long) 0x7a1f4a8a;

  @Override
  public boolean AddHash32(int hash) {
    long hash64 = (((REHASH_32 * (long) hash) >>> 32) << 32) | hash;
    int bucket_idx = Index(hash64, payload.length / 8);
    int[] mask = {0xca11, 8, 6, 7, 5, 3, 0, 9};
    MakeMask(hash, mask);
    for (int i = 0; i < 8; ++i) {
      payload[bucket_idx * 8 + i] = mask[i] | payload[bucket_idx * 8 + i];
    }
    return true;
  }

  @Override
  public boolean FindHash32(int hash) {
    long hash64 = (((REHASH_32 * (long) hash) >>> 32) << 32) | hash;
    int bucket_idx = Index(hash64, payload.length / 8);
    int[] mask = {0xca11, 8, 6, 7, 5, 3, 0, 9};
    MakeMask(hash, mask);
    for (int i = 0; i < 8; ++i) {
      if (0 == (payload[bucket_idx * 8 + i] & mask[i])) return false;
    }
    return true;
  }

}
