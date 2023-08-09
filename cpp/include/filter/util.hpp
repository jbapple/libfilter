#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>

#if defined(__LZCNT__)
#include <immintrin.h>
#endif

namespace filter {
namespace detail {

#define INLINE __attribute__((always_inline)) inline

// Returns the lowest k bits of x
INLINE constexpr uint64_t Mask(int w, uint64_t x) { return x & ((1ul << w) - 1); }

struct PcgRandom {
  // *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
  // Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

  int bit_width;  // The only construction parameter - how many bits to slice off the rng
                  // each time. IOW, this will give you up to 32 bits at a time. How many
                  // do you need? We can save RNG calls by caching the remainder
  uint64_t state = 0x13d26df6f74044b3;
  uint64_t inc = 0x0d09b2d3025545a0;

  uint32_t current = 0;
  int remaining_bits = 0;

  INLINE uint32_t Get() {
    // Save some bits for next time
    if (remaining_bits >= bit_width) {
      auto result = Mask(bit_width, current);
      current = current >> bit_width;
      remaining_bits = remaining_bits - bit_width;
      return result;
    }

    uint64_t oldstate = state;
    // Advance internal state
    state = oldstate * 6364136223846793005ULL + (inc | 1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    current = (xorshifted >> rot) | (xorshifted << ((-rot) & 31));

    remaining_bits = 32 - bit_width;
    auto result = Mask(bit_width, current);
    current = current >> bit_width;
    return result;
  }
};

// Given non-zero x and y, consider each as a sequence of up to 15 [sic] bits. The length
// of the sequence is encoded in the lower order bits. specifically, if the lowest n bits
// are zero and the next higher bit is one, then the sequence encoded has length 15 - n. It
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
#if not defined(__LZCNT__)
constexpr
#endif
INLINE bool IsPrefixOf(uint16_t x, uint16_t y) {
  assert(x != 0);
  assert(y != 0);
  auto a = x ^ y;
  auto c = __builtin_ctz(x);
  auto h = __builtin_ctz(y);
#if defined(__LZCNT__)
  int i = _lzcnt_u32(a);
#else
  auto i = (a == 0) ? 32 : __builtin_clz(a);
#endif
  return (c >= h) && (i >= 31 - c);
}

// These static asserts are forbidden by the use of _lzcnt_u32

#if not defined(__LZCNT__)
static_assert(IsPrefixOf(2, 1), "IsPrefixOf(2, 1)");
static_assert(IsPrefixOf(2, 3), "IsPrefixOf(2, 3)");
static_assert(IsPrefixOf(4, 1), "IsPrefixOf(4, 1)");

static_assert(not IsPrefixOf(1, 3), "IsPrefixOf(1, 3)");
static_assert(not IsPrefixOf(1, 2), "IsPrefixOf(1, 2)");

static_assert(not IsPrefixOf(3, 1), "IsPrefixOf(3, 1)");
static_assert(not IsPrefixOf(3, 2), "IsPrefixOf(3, 2)");
static_assert(not IsPrefixOf(5, 2), "IsPrefixOf(5, 2)");
static_assert(not IsPrefixOf(6, 2), "IsPrefixOf(6, 2)");
static_assert(not IsPrefixOf(7, 2), "IsPrefixOf(7, 2)");

static_assert(not IsPrefixOf(2, 5), "IsPrefixOf(2, 5)");
static_assert(not IsPrefixOf(2, 6), "IsPrefixOf(2, 6)");
static_assert(not IsPrefixOf(2, 7), "IsPrefixOf(2, 7)");

static_assert(IsPrefixOf(16384, 1), "IsPrefixOf(16384, 1)");
#endif

// Return the combined value if x and y are the same except for their last
// digit. Can be used during insert to elide some inserts by setting their prefix as the
// value in the table slot, but rarely combinable, so doesn't seem to make much of a
// difference. The reason is that most of the keys have long tails, and long tails only
// match some small percent of the time.
//
// Returns 0 if no match
INLINE constexpr uint16_t Combinable(uint16_t x, uint16_t y) {
  // assert x != y, x != 0, y != 0, x >> 15 == 0, y >> 15 == 0
  uint32_t xy = x ^ y;
  uint32_t low = __builtin_ctz(xy);
  uint32_t high = __builtin_clz(xy);
  uint32_t xlow = __builtin_ctz(x);
  uint32_t ylow = __builtin_ctz(y);
  if (low + high == 31 && xlow == ylow && xlow  + 1 == low) {
    // This depends on the width of x and y to be 15 bits or less.
    return (x + y) / 2;
  }
  return 0;
}

static_assert(Combinable(1, 3) == 2, "Combinable(1, 3)");
static_assert(Combinable(5, 7) == 6, "Combinable(5, 7)");
static_assert(Combinable(2, 6) == 4, "Combinable(2, 6)");
static_assert(Combinable(1, 5) == 0, "Combinable(1, 5)");
static_assert(Combinable(1, 6) == 0, "Combinable(1, 6)");

}  // namespace detail
}  // namespace filter
