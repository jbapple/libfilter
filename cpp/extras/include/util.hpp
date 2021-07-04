#pragma once

#include <cstdint>
#include <random>


// Generator for unsigned 64-bit ints, initialized from system's randomness generator.
class Rand {
  //===== RomuDuoJr==============================================================
  //
  // The fastest generator using 64-bit arith., but not suited for huge jobs.
  // Est. capacity = 2^51 bytes. Register pressure = 4. State size = 128 bits.
public:
  static uint64_t rotl(uint64_t d, int lrot) {
    return (d << (lrot)) | (d >> (8 * sizeof(d) - (lrot)));
  }

  uint64_t xState, yState;  // set to nonzero seed
  uint64_t initXState, initYState;  // set to nonzero seed

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

    // xState = 0x3220c57100d65213;
    // yState = 0x5756c9a907a58d43;
    // xState = 0x2b6832430098251b;
    // yState = 0x21405e54f7972f34;
    initXState = xState;
    initYState = yState;
    // xState = 0x2842f167de69e747;
    // yState = 0x2842f167de69e747;
    // xState = 0x306cbabbb4c34e88;
    // yState = 0x0a0718e1416341e3;
    std::cout << std::hex << "0x" << xState << std::endl;
    std::cout << std::hex << "0x" << yState << std::endl;
  }

  std::uint64_t operator()() { return romuDuoJr_random(); }

  ~Rand() {
    std::cout << std::hex << "0x" << initXState << std::endl;
    std::cout << std::hex << "0x" << initYState << std::endl;
  }
};
