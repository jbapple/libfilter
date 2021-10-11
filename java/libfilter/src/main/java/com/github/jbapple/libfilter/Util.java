package com.github.jbapple.libfilter;

// Feistel is a permutation that is also a hash function, based on a Feistel permutation.
class Feistel implements Cloneable {
  // The salt for the hash functions. The component hash function is strong
  // multiply-shift.
  long[] keys;

  @Override
  public Feistel clone() {
    Feistel result = new Feistel(keys);
    result.keys = keys.clone();
    return result;
  }

  static long Mask(int w, long x) { return x & ((((long) 1) << w) - 1); }

  static long Lo(int s, int t, int w, long x) { return Mask(w, x); }
  static long Hi(int s, int t, int w, long x) { return Mask(w, x >>> (s + t - w)); }

  Feistel(long[] entropy) { keys = entropy; }

  // Applies strong multiply-shift to the w low bits of x, returning the high w bits
  long ApplyOnce(int s, int t, int w, long x, int offset) {
    return Hi(s, t, s + t - w,
        Mask(w, x) * Mask(s + t, keys[offset + 0]) + Mask(s + t, keys[offset + 1]));
  }

  // Performs the hash function "forwards". w is the width of x. This is ASYMMETRIC Feistel.
  long Permute(int w, long x) {
    // s is floor(w/2), t is ceil(w/2).
    int s = w >>> 1;
    int t = w - s;

    // break up x into the low s bits and the high t bits
    long l0 = Lo(s, t, s, x);
    long r0 = Hi(s, t, t, x);

    // First Feistel application: switch the halves. l1 has t bits, while r1 has s bits,
    // xored with the t-bit hash of r0.
    long l1 = r0; // t
    long r1 = l0 ^ ApplyOnce(s, t, t, r0, 0); // s

    // l2 has t bits. r2 has t-bits, xored with the s-bit hash of r1, which really has t
    // bits. This is permitted because strong-multiply shift is still strong if you mask
    // it.
    long l2 = r1; // s
    long r2 = l1 ^ ApplyOnce(s, t, s, r1, 2); // t

    // The validity of this is really only seen when understanding the reverse permutation
    long result = (r2 << s) | l2;
    return result;
  }

  long ReversePermute(int w, long x)  {
    int s = w / 2;
    int t = w - s;

    long l2 = Lo(s, t, s, x);
    long r2 = Hi(s, t, t, x);

    long r1 = l2;                                    // s
    long l1 = r2 ^ ApplyOnce(s, t, s, r1, 2);  // t

    long r0 = l1;                                    // t
    long l0 = r1 ^ ApplyOnce(s, t, t, r0, 0);  // s

    long result = (r0 << s) | l0;
    return result;
  }

  long Summary() { return keys[0] ^ keys[1] ^ keys[2] ^ keys[3]; }
}

class PcgRandom implements Cloneable {
  // *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
  // Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

  final int
      bitWidth; // The only construction parameter - how many bits to slice off the rng
                // each time. IOW, this will give you up to 32 bits at a time. How many
                // do you need? We can save RNG calls by caching the remainder
  long state;
  long inc;

  int current;
  int remainingBits;

  @Override
  public PcgRandom clone() {
    PcgRandom result = new PcgRandom(bitWidth);
    result.state = state;
    result.inc = inc;
    result.current = current;
    result.remainingBits = remainingBits;
    return result;
  }

  PcgRandom(int bw) {
    bitWidth = bw;
    state = 0x13d26df6f74044b3L;
    inc = 0x0d09b2d3025545a0L;
    current = 0;
    remainingBits = 0;
  }

  int Get() {
    // Save some bits for next time
    if (remainingBits >= bitWidth) {
      long result = Feistel.Mask(bitWidth, current);
      current = current >>> bitWidth;
      remainingBits = remainingBits - bitWidth;
      return (int)result;
    }

    long oldState = state;
    // Advance internal state
    state = oldState * 6364136223846793005L + (inc | 1);
    // Calculate output function (XSH RR), uses old state for max ILP
    int xorShifted = (int)(((oldState >> 18) ^ oldState) >> 27);
    int rot = (int)(oldState >> 59);
    current = (xorShifted >> rot) | (xorShifted << ((-rot) & 31));

    remainingBits = 32 - bitWidth;
    long result = Feistel.Mask(bitWidth, current);
    current = current >> bitWidth;
    return (int)result;
  }
}

class Util {
  static boolean IsPrefixOf(short x, short y) {
    // assert(x != 0);
    // assert(y != 0);
    int a = x ^ y;
    int c = Integer.numberOfTrailingZeros(Short.toUnsignedInt(x));
    int h = Integer.numberOfTrailingZeros(Short.toUnsignedInt(y));
    int i = Integer.numberOfLeadingZeros(a);
    return (c >= h) && (i >= 31 - c);
  }
}
