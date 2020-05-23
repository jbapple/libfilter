#pragma once

#include <cstdint>
#include <random>

extern "C" {
#include "pcg_variants.h"
}

// Generator for unsigned 64-bit ints, initialized from system's randomness generator.
class Rand {
  pcg64s_random_t r;

 public:
  Rand() {
    std::random_device raw("/dev/urandom");
    constexpr auto kDigits = std::numeric_limits<std::random_device::result_type>::digits;
    pcg128_t seed = raw();
    seed = seed << kDigits;
    seed = seed | raw();
    seed = seed << kDigits;
    seed = seed | raw();
    seed = seed << kDigits;
    seed = seed | raw();
    pcg64s_srandom_r(&r, seed);
  }
  std::uint64_t operator()() { return pcg64s_random_r(&r); }
};
