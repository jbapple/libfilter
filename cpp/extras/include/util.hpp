#pragma once

#include <cstdint>
#include <random>


// Generator for unsigned 64-bit ints, initialized from system's randomness generator.
class Rand {
  //===== RomuDuoJr==============================================================
  //
  // The fastest generator using 64-bit arith., but not suited for huge jobs.
  // Est. capacity = 2^51 bytes. Register pressure = 4. State size = 128 bits.

  static uint64_t rotl(uint64_t d, int lrot) {
    return (d << (lrot)) | (d >> (8 * sizeof(d) - (lrot)));
  }

  uint64_t xState, yState;  // set to nonzero seed

  uint64_t romuDuoJr_random() {
    uint64_t xp = xState;
    xState = 15241094284759029579u * yState;
    yState = yState - xp;
    yState = rotl(yState, 27);
    return xp;
  }

 public:
  Rand() {
    std::random_device raw("/dev/urandom");
    // constexpr auto kDigits = std::numeric_limits<std::random_device::result_type>::digits;

    xState = raw();
    xState = xState << 32;
    xState |= raw();

    yState = raw();
    yState = yState << 32;
    yState |= raw();

    xState = 0x306cbabbb4c34e88;
    yState = 0x0a0718e1416341e6;
  }

  std::uint64_t operator()() { return romuDuoJr_random(); }
};
