#pragma once

#include <cstdint>
#include <iostream>
#include <random>

using namespace std;

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

    xState = raw();
    xState = xState << 32;
    xState |= raw();

    yState = raw();
    yState = yState << 32;
    yState |= raw();

    // 5
    // xState = 0x65d669960f333899;
    // yState = 0x74356b1ce0304637;

    // 4
    // xState = 0xd577603879438a27;
    // yState = 0xe2679c8afa199898;

    // 8
    // xState = 0x46d18fbe1e4ebc67;
    // yState = 0x941d540452ab38aa;

    // 1 14
    // 64 58
    // xState = 0x25432790d7657885;
    // yState = 0x6b4ce3189fd1dc1f;

    // 62 28
    // xState = 0x413db91a87e812c8;
    // yState = 0x44b9fde1f7da9d96;

    // ndv 14 12
    // 300a7748208328a4
    // 2da460f286315a88

    // ndv 11 12
    // d47e8e2365691c80
    // e4861292dabc35a3

    // ndv 10 10
    // 9340b1b8521d6da7
    // 2adb57414584700e

    // ndv 9 9
    // xState = 0x5389ca9d7e980a03;
    // yState = 0x87d5ae2ea5ff2a26;

    // 1 462
    // xState = 0xf7eed0b4b899dcda;
    // yState = 0x8c9635cf8a30737e;

    // 1 911
    // xState = 0x8541a193bf58d3ae;
    // yState = 0xe182f6b580d4dece;

    //13 469
    xState = 0x270c120999ddf1c9;
    yState = 0xaf4f1d8c386ef733;

    initXState = xState;
    initYState = yState;
    cout << hex << xState << "\n" << yState << endl;
  }

  std::uint64_t operator()() { return romuDuoJr_random(); }
};
