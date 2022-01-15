#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#if defined(__LZCNT__)
#include <immintrin.h>
#endif

#define INLINE __attribute__((always_inline)) inline

// Returns the lowest k bits of x
INLINE uint64_t libfilter_mask(int w, uint64_t x) { return x & ((1ul << w) - 1); }

// Returns the w high bits of x, after s+t-w low bits
INLINE uint64_t libfilter_high_bits(int s, int t, int w, uint64_t x) {
  return libfilter_mask(w, x >> (s + t - w));
}

// Applies strong multiply-shift to the w low bits of x, returning the high s+t-w bits
INLINE uint64_t libfilter_feistel_subhash(int s, int t, int w, uint64_t x,
                                                 const uint64_t k[2]) {
  return libfilter_high_bits(
      s, t, s + t - w,
      libfilter_mask(w, x) * libfilter_mask(s + t, k[0]) + libfilter_mask(s + t, k[1]));
}

// Feistel is a permutation that is also a hash function
typedef struct {
  // The salt for the hash functions. The component hash function is strong
  // multiply-shift.
  uint64_t keys[2][2];
} libfilter_feistel;

INLINE libfilter_feistel libfilter_feistel_create(const uint64_t entropy[4]) {
  libfilter_feistel result;
  result.keys[0][0] = entropy[0];
  result.keys[0][1] = entropy[1];
  result.keys[1][0] = entropy[2];
  result.keys[1][1] = entropy[3];
  return result;
}

// Performs the hash function "forwards". w is the width of x. This is ASYMMETRIC Feistel.
INLINE uint64_t libfilter_feistel_permute_forward(const libfilter_feistel *here, int w,
                                                  uint64_t x) {
  // s is floor(w/2), t is ceil(w/2).
  int s = w >> 1;
  int t = w - s;

  // break up x into the low s bits and the high t bits
  uint64_t l0 = libfilter_mask(s, x);
  uint64_t r0 = libfilter_high_bits(s, t, t, x);

  // First feistel application: switch the halves. l1 has t bits, while r1 has s bits,
  // xored with the t-bit hash of r0.
  uint64_t l1 = r0;                                                          // t
  uint64_t r1 = l0 ^ libfilter_feistel_subhash(s, t, t, r0, here->keys[0]);  // s

  // l2 has t bits. r2 has t-bits, xored with the s-bit hash of r1, which really has t
  // bits. This is permitted because strong-multiply shift is still strong if you mask
  // it.
  uint64_t l2 = r1;                                                          // s
  uint64_t r2 = l1 ^ libfilter_feistel_subhash(s, t, s, r1, here->keys[1]);  // t

  // The validity of this is really only seen when understanding the reverse permutation
  uint64_t result = (r2 << s) | l2;
  return result;
}

INLINE uint64_t libfilter_feistel_permute_backward(const libfilter_feistel *here, int w,
                                                   uint64_t x) {
  int s = w / 2;
  int t = w - s;

  uint64_t l2 = libfilter_mask(s, x);
  uint64_t r2 = libfilter_high_bits(s, t, t, x);

  uint64_t r1 = l2;                                                          // s
  uint64_t l1 = r2 ^ libfilter_feistel_subhash(s, t, s, r1, here->keys[1]);  // t

  uint64_t r0 = l1;                                                          // t
  uint64_t l0 = r1 ^ libfilter_feistel_subhash(s, t, t, r0, here->keys[0]);  // s

  uint64_t result = (r0 << s) | l0;
  return result;
}

//  friend void swap(detail_Feistel&, detail_Feistel&);

// std::size_t Summary() const {
//   return keys[0][0] ^ keys[0][1] ^ keys[1][0] ^ keys[1][1];
// }
// Feistel(const Feistel&) = delete;
// Feistel& operator=(const Feistel&) = delete;

// INLINE void swap(detail_Feistel& x, detail_Feistel& y) {
//   for (int i = 0; i < 2; ++i) {
//     for (int j = 0; j < 2; ++j) {
//       using std::swap;
//       swap(x.keys[i][j], y.keys[i][j]);
//     }
//   }
// }

typedef struct {
  // *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
  // Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

  int bit_width;  // The only construction parameter - how many bits to slice off the rng
                  // each time. IOW, this will give you up to 32 bits at a time. How many
                  // do you need? We can save RNG calls by caching the remainder
  uint64_t state;
  uint64_t inc;

  uint32_t current;
  int remaining_bits;
} libfilter_pcg_random;

INLINE libfilter_pcg_random libfilter_pcg_random_create(int bit_width) {
  libfilter_pcg_random result;
  result.bit_width = bit_width;
  result.state = 0x13d26df6f74044b3;
  result.inc = 0x0d09b2d3025545a0;
  result.current = 0;
  result.remaining_bits = 0;
  return result;
}

INLINE uint32_t libfilter_pcg_random_get(libfilter_pcg_random *here) {
  // Save some bits for next time
  if (here->remaining_bits >= here->bit_width) {
    uint32_t result = libfilter_mask(here->bit_width, here->current);
    here->current = here->current >> here->bit_width;
    here->remaining_bits = here->remaining_bits - here->bit_width;
    return result;
  }

  uint64_t oldstate = here->state;
  // Advance internal state
  here->state = oldstate * 6364136223846793005ULL + (here->inc | 1);
  // Calculate output function (XSH RR), uses old state for max ILP
  uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
  uint32_t rot = oldstate >> 59u;
  here->current = (xorshifted >> rot) | (xorshifted << ((-rot) & 31));

  here->remaining_bits = 32 - here->bit_width;
  uint32_t result = libfilter_mask(here->bit_width, here->current);
  here->current = here->current >> here->bit_width;
  return result;
}

// Given non-zero x and y, consider eash as a sequence of up to 15 [sic] bits. The length
// of the sequence is encoded in the lower order bits. specifically, if the lowest n bits
// are zero and the next higher bit is one, then the sequence encoded has length15 - n. It
// is stored in the high-order bits of the uint16_t. Examples, using 8 bits:
//
// 01010101 is the sequence 0101010
// 11001100 is the sequence 11001
// 00000000 is invalid
// 10000000 is the empty sequence
//
// Given all that, consider non-zero x and y as valid sequences. IsPrefixOf returns true
// if an only if the sequence represented by x can be extended to the sequence represented
// by y.
INLINE bool libfilter_taffy_is_prefix_of(uint16_t x, uint16_t y) {
  assert(x != 0);
  assert(y != 0);
  uint16_t a = x ^ y;
  int c = __builtin_ctz(x);
  int h = __builtin_ctz(y);
#if defined(__LZCNT__)
  int i = _lzcnt_u32(a);
#else
  int i = (a == 0) ? 32 : __builtin_clz(a);
#endif
  return (c >= h) && (i >= 31 - c);
}

// These static asserts are forbidden by the use of _lzcnt_u32

// #if not defined(__LZCNT__)
// static_assert(IsPrefixOf(2, 1), "IsPrefixOf(2, 1)");
// static_assert(IsPrefixOf(2, 3), "IsPrefixOf(2, 3)");
// static_assert(IsPrefixOf(4, 1), "IsPrefixOf(4, 1)");

// static_assert(not IsPrefixOf(1, 3), "IsPrefixOf(1, 3)");
// static_assert(not IsPrefixOf(1, 2), "IsPrefixOf(1, 2)");

// static_assert(not IsPrefixOf(3, 1), "IsPrefixOf(3, 1)");
// static_assert(not IsPrefixOf(3, 2), "IsPrefixOf(3, 2)");
// static_assert(not IsPrefixOf(5, 2), "IsPrefixOf(5, 2)");
// static_assert(not IsPrefixOf(6, 2), "IsPrefixOf(6, 2)");
// static_assert(not IsPrefixOf(7, 2), "IsPrefixOf(7, 2)");

// static_assert(not IsPrefixOf(2, 5), "IsPrefixOf(2, 5)");
// static_assert(not IsPrefixOf(2, 6), "IsPrefixOf(2, 6)");
// static_assert(not IsPrefixOf(2, 7), "IsPrefixOf(2, 7)");

// static_assert(IsPrefixOf(16384, 1), "IsPrefixOf(16384, 1)");
// #endif

// Return the combined value if x and y are the same except for their last
// digit. Can be used during insert to elide some inserts by setting their prefix as the
// value in the table slot, but rarely combinable, so doesn't seem to make much of a
// difference. The reason is that most of the keys have long tails, and long tails only
// match some small percent of the time.
//
// Returns 0 if no match
INLINE uint16_t libfilter_taffy_tail_pair(uint16_t x, uint16_t y) {
  // assert x != y, x != 0, y != 0, x >> 15 == 0, y >> 15 == 0
  uint32_t xy = x ^ y;
  uint32_t low = __builtin_ctz(xy);
  uint32_t high = __builtin_clz(xy);
  uint32_t xlow = __builtin_ctz(x);
  uint32_t ylow = __builtin_ctz(y);
  if (low + high == 31 && xlow == ylow && xlow + 1 == low) {
    // This depends on the width of x and y to be 15 bits or less.
    return (x + y) / 2;
  }
  return 0;
}

// static_assert(Combinable(1, 3) == 2, "Combinable(1, 3)");
// static_assert(Combinable(5, 7) == 6, "Combinable(5, 7)");
// static_assert(Combinable(2, 6) == 4, "Combinable(2, 6)");
// static_assert(Combinable(1, 5) == 0, "Combinable(1, 5)");
// static_assert(Combinable(1, 6) == 0, "Combinable(1, 6)");
