// This header defines HalftimeHash, a hash function designed for long strings (> 1KB).
//
// HalftimeHash is a descendant of Badger and EHC. The article "HalftimeHash: modern
// hashing without 64-bit multipliers or finite fields" describes it in more detail.

#if defined(__x86_64)
#include <immintrin.h>
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

// NEON and SSE2 use the same type name. This is lazy, but it reduces code size and
// shouldn't be a problem unless there is an arch that supports both (there isn't) or
// until this code has dynamic CPU dispatch.

#if defined(__ARM_NEON) || defined(__ARM_NEON__)

typedef uint64x2_t libfilter_u64x2;

inline libfilter_u64x2 libfilter_hash_leftshift(libfilter_u64x2 a, int i) {
  return vshlq_s64(a, vdupq_n_s64(i));
}
inline libfilter_u64x2 libfilter_hash_plus(libfilter_u64x2 a, libfilter_u64x2 b) {
  return vaddq_s64(a, b);
}
inline libfilter_u64x2 libfilter_hash_plus32(libfilter_u64x2 a, libfilter_u64x2 b) {
  return vaddq_s32(a, b);
}
inline libfilter_u64x2 libfilter_hash_righshift32(libfilter_u64x2 a) {
  return vshrq_n_u64(a, 32);
}
inline libfilter_u64x2 libfilter_hash_times(libfilter_u64x2 a, libfilter_u64x2 b) {
  uint32x2_t a_lo = vmovn_u64(a);
  uint32x2_t b_lo = vmovn_u64(b);
  return vmull_u32(a_lo, b_lo);
}
inline libfilter_u64x2 libfilter_hash_xor(libfilter_u64x2 a, libfilter_u64x2 b) {
  return veorq_s32(a, b);
}
inline uint64_t libfilter_hash_sum(libfilter_u64x2 a) {
  return vgetq_lane_s64(a, 0) + vgetq_lane_s64(a, 1);
}
inline libfilter_u64x2 libfilter_hash_load_block(const void* x) {
  const int32_t* y = (const int32_t*)(x);
  return vld1q_s32(y);
}
inline libfilter_u64x2 libfilter_hash_load_one(uint64_t entropy) {
  return vdupq_n_s64(entropy);
}
libfilter_u64x2 libfilter_hash_multiply_add(libfilter_u64x2 summand,
                                            libfilter_u64x2 factor1,
                                            libfilter_u64x2 factor2) {
  return vmlal_u32(summand, vmovn_u64(factor1), vmovn_u64(factor2));
}

#elif __SSE2__

typedef __m128i libfilter_u64x2;

inline libfilter_u64x2 libfilter_hash_leftshift(libfilter_u64x2 a, int i) {
  return _mm_slli_epi64(a, i);
}
inline libfilter_u64x2 libfilter_hash_plus(libfilter_u64x2 a, libfilter_u64x2 b) {
  return _mm_add_epi64(a, b);
}
inline libfilter_u64x2 libfilter_hash_plus32(libfilter_u64x2 a, libfilter_u64x2 b) {
  return _mm_add_epi32(a, b);
}
inline libfilter_u64x2 libfilter_hash_righshift32(libfilter_u64x2 a) {
  return _mm_srli_epi64(a, 32);
}
inline libfilter_u64x2 libfilter_hash_times(libfilter_u64x2 a, libfilter_u64x2 b) {
  return _mm_mul_epu32(a, b);
}
inline libfilter_u64x2 libfilter_hash_xor(libfilter_u64x2 a, libfilter_u64x2 b) {
  return _mm_xor_si128(a, b);
}

inline uint64_t libfilter_hash_sum(libfilter_u64x2 a) { return a[0] + a[1]; }

inline libfilter_u64x2 libfilter_hash_load_block(const void* x) {
  const libfilter_u64x2* y = (const libfilter_u64x2*)x;
  return _mm_loadu_si128(y);
}

inline libfilter_u64x2 libfilter_hash_load_one(uint64_t entropy) {
  return _mm_set1_epi64x(entropy);
}

libfilter_u64x2 libfilter_hash_multiply_add(libfilter_u64x2 summand,
                                            libfilter_u64x2 factor1,
                                            libfilter_u64x2 factor2) {
  return libfilter_hash_plus(summand, libfilter_hash_times(factor1, factor2));
}

#else

typedef struct alignas(sizeof(uint64_t) * 2) {
  uint64_t it[2];
} libfilter_u64x2;

inline uint64_t libfilter_hash_plus32(uint64_t a, uint64_t b) {
  uint64_t result;
  uint32_t temp[2] = {(uint32_t)a + (uint32_t)b,
                      (uint32_t)(a >> 32) + (uint32_t)(b >> 32)};
  memcpy(&result, temp, sizeof(result));
  return result;
}

inline uint64_t libfilter_hash_times_small(uint64_t a, uint64_t b) {
  const uint64_t mask = (((uint64_t)1) << 32) - 1;
  return (a & mask) * (b & mask);
}

inline uint64_t libfilter_hash_load_block_small(const void* x) {
  const char* y = (const char*)(x);
  uint64_t result;
  memcpy(&result, y, sizeof(uint64_t));
  return result;
}

inline libfilter_u64x2 libfilter_hash_load_one(uint64_t entropy) {
  uint64_t result;
  for (unsigned i = 0; i < 2; ++i) {
    result.it[i] = entropy;
  }
  return result;
}

inline uint64_t libfilter_hash_load_block(const void* x) {
  const char* y = (const char*)x;
  uint64_t result;
  for (unsigned i = 0; i < 2; ++i) {
    result.it[i] = libfilter_hash_load_block_small(y + i * sizeof(uint64_t));
  }
  return result;
}

inline libfilter_u64x2 libfilter_hash_xor(libfilter_u64x2 a, libfilter_u64x2 b) {
  libfilter_u64x2 result;
  for (unsigned i = 0; i < 2; ++i) {
    result.it[i] = a.it[i] ^ b.it[i];
  }
  return result;
}

inline libfilter_u64x2 libfilter_hash_plus32(libfilter_u64x2 a, libfilter_u64x2 b) {
  libfilter_u64x2 result;
  for (unsigned i = 0; i < 2; ++i) {
    result.it[i] = libfilter_hash_plus32_small(a.it[i], b.it[i]);
  }
  return result;
}

inline libfilter_u64x2 libfilter_hash_plus(libfilter_u64x2 a, libfilter_u64x2 b) {
  libfilter_u64x2 result;
  for (unsigned i = 0; i < 2; ++i) {
    result.it[i] = a.it[i] + b.it[i];
  }
  return result;
}

inline libfilter_u64x2 libfilter_hash_leftshift(libfilter_u64x2 a, int s) {
  libfilter_u64x2 result;
  for (unsigned i = 0; i < 2; ++i) {
    result.it[i] = a.it[i] << s;
  }
  return result;
}

inline libfilter_u64x2 libfilter_hash_righshift32(libfilter_u64x2 a) {
  libfilter_u64x2 result;
  for (unsigned i = 0; i < 2; ++i) {
    result.it[i] = a.it[i] >> 32;
  }
  return result;
}

inline libfilter_u64x2 libfilter_hash_times(libfilter_u64x2 a, libfilter_u64x2 b) {
  libfilter_u64x2 result;
  for (unsigned i = 0; i < 2; ++i) {
    result.it[i] = libfilter_hash_times_small(a.it[i], b.it[i]);
  }
  return result;
}

inline uint64_t libfilter_hash_sum(libfilter_u64x2 a) {
  uint64_t result = 0;
  for (unsigned i = 0; i < 2; ++i) {
    result += a.it[i];
  }
  return result;
}

libfilter_u64x2 libfilter_hash_multiply_add(libfilter_u64x2 summand,
                                            libfilter_u64x2 factor1,
                                            libfilter_u64x2 factor2) {
  return libfilter_hash_plus(summand, libfilter_hash_times(factor1, factor2));
}

#endif

// Here begin the encoding functions as part of EHC. Each takes an array of size
// `encoded_dimension * in_width` as an argument. Only the first `dimension * in_width`
// values are present.  The remaining ones are populated so as to make an erasure code
// with minimum distance `encoded_dimension - dimension`.
//
// Each of these (except the trivial use case) uses an erasure code from Emin Gabrielyan.
//
// https://docs.switzernet.com/people/emin-gabrielyan/051101-erasure-9-7-resilient/
inline void libfilter_hash_ehc_encode(libfilter_u64x2 io[9][3]) {
  const unsigned x = 0, y = 1, z = 2;

  const libfilter_u64x2* iter = io[0];
  io[7][x] = io[8][x] = iter[x];
  io[7][y] = io[8][y] = iter[y];
  io[7][z] = io[8][z] = iter[z];
  iter += 1;

#define DistributeRaw1(slot, label, a) \
  io[slot][a] = libfilter_hash_xor(io[slot][a], iter[label])

#define DistributeRaw2(slot, label, a, b)                     \
  io[slot][a] = libfilter_hash_xor(io[slot][a], iter[label]); \
  io[slot][b] = libfilter_hash_xor(io[slot][b], iter[label])

#define DistributeRaw3(slot, label, a, b, c) \
  DistributeRaw2(slot, label, a, b);         \
  io[slot][c] = libfilter_hash_xor(io[slot][c], iter[label])

#define Distribute3(idx, a, b, c) \
  DistributeRaw1(idx, x, a);      \
  DistributeRaw1(idx, y, b);      \
  DistributeRaw1(idx, z, c);      \
  iter += 1;

  while (iter != io[9]) {
    Distribute3(7, x, y, z);
  }

  iter = io[1];

  DistributeRaw1(8, x, z);
  DistributeRaw2(8, y, x, z);
  DistributeRaw1(8, z, y);
  ++iter;

  DistributeRaw2(8, x, x, z);
  DistributeRaw3(8, y, x, y, z);
  DistributeRaw2(8, z, y, z);
  ++iter;

  DistributeRaw1(8, x, y);
  DistributeRaw2(8, y, y, z);
  DistributeRaw2(8, z, x, z);
  ++iter;

  DistributeRaw2(8, x, x, y);
  DistributeRaw1(8, y, z);
  DistributeRaw1(8, z, x);
  ++iter;

  DistributeRaw2(8, x, y, z);
  DistributeRaw2(8, y, x, y);
  DistributeRaw3(8, z, x, y, z);
  ++iter;

  DistributeRaw3(8, x, x, y, z);
  DistributeRaw1(8, y, x);
  DistributeRaw2(8, z, x, y);
}

// evenness: 2 weight: 10
//  0   0   1   4   1   1   2   2   1
//  1   1   0   0   1   4   1   2   2
//  1   4   1   1   0   0   2   1   2

inline void libfilter_hash_ehc_combine(const libfilter_u64x2 input[9],
                                       libfilter_u64x2 output[3]) {
  output[1] = input[0];
  output[2] = input[0];

  output[1] = libfilter_hash_plus(output[1], input[1]);
  output[2] = libfilter_hash_plus(output[2], libfilter_hash_leftshift(input[1], 2));

  output[0] = input[2];
  output[2] = libfilter_hash_plus(output[2], input[2]);

  output[0] = libfilter_hash_plus(output[0], libfilter_hash_leftshift(input[3], 2));
  output[2] = libfilter_hash_plus(output[2], input[3]);

  output[0] = libfilter_hash_plus(output[0], input[4]);
  output[1] = libfilter_hash_plus(output[1], input[4]);

  output[0] = libfilter_hash_plus(output[0], input[5]);
  output[1] = libfilter_hash_plus(output[1], libfilter_hash_leftshift(input[5], 2));

  output[0] = libfilter_hash_plus(output[0], libfilter_hash_leftshift(input[6], 1));
  output[1] = libfilter_hash_plus(output[1], input[6]);
  output[2] = libfilter_hash_plus(output[2], libfilter_hash_leftshift(input[6], 1));

  output[0] = libfilter_hash_plus(output[0], libfilter_hash_leftshift(input[7], 1));
  output[1] = libfilter_hash_plus(output[1], libfilter_hash_leftshift(input[7], 1));
  output[2] = libfilter_hash_plus(output[2], input[7]);

  output[0] = libfilter_hash_plus(output[0], input[8]);
  output[1] = libfilter_hash_plus(output[1], libfilter_hash_leftshift(input[8], 1));
  output[2] = libfilter_hash_plus(output[2], libfilter_hash_leftshift(input[8], 1));
}

inline uint64_t FloorLog(uint64_t a, uint64_t b) {
  return (0 == a) ? 0 : ((b < a) ? 0 : (1 + (FloorLog(a, b / a))));
}

inline libfilter_u64x2 libfilter_hash_mix(libfilter_u64x2 accum, libfilter_u64x2 input,
                                          libfilter_u64x2 entropy) {
  libfilter_u64x2 output = libfilter_hash_plus32(entropy, input);
  libfilter_u64x2 twin = libfilter_hash_righshift32(output);
  output = libfilter_hash_multiply_add(accum, output, twin);
  return output;
}

inline libfilter_u64x2 libfilter_hash_mix_one(libfilter_u64x2 accum,
                                              libfilter_u64x2 input, uint64_t entropy) {
  return libfilter_hash_mix(accum, input, libfilter_hash_load_one(entropy));
}

inline libfilter_u64x2 libfilter_hash_mix_none(libfilter_u64x2 input,
                                               uint64_t entropy_word) {
  libfilter_u64x2 entropy = libfilter_hash_load_one(entropy_word);
  libfilter_u64x2 output = libfilter_hash_plus32(entropy, input);
  libfilter_u64x2 twin = libfilter_hash_righshift32(output);
  output = libfilter_hash_times(output, twin);
  return output;
}

inline void libfilter_hash_upper(const libfilter_u64x2 input[8][3],
                                 const uint64_t entropy[3 * (8 - 1)],
                                 libfilter_u64x2 output[3]) {
  for (unsigned i = 0; i < 3; ++i) {
    output[i] = input[0][i];
    for (unsigned j = 1; j < 8; ++j) {
      output[i] =
          libfilter_hash_mix_one(output[i], input[j][i], entropy[(8 - 1) * i + j - 1]);
    }
  }
}

inline void libfilter_hash_ehc_load(const char input[7 * 3 * sizeof(libfilter_u64x2)],
                                    libfilter_u64x2 output[7][3]) {
  static_assert(7 * 3 <= 28, "");
#if !defined(__clang__)
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC unroll 28
#endif
#else
#pragma unroll
#endif
  for (unsigned i = 0; i < 7; ++i) {
#if !defined(__clang__)
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC unroll 28
#endif
#else
#pragma unroll
#endif
    for (unsigned j = 0; j < 3; ++j) {
      output[i][j] =
          libfilter_hash_load_block(&input[(i * 3 + j) * sizeof(libfilter_u64x2)]);
    }
  }
}

inline void libfilter_hash_ehc_hash(const libfilter_u64x2 input[9][3],
                                    const uint64_t entropy[9][3],
                                    libfilter_u64x2 output[9]) {
  for (unsigned i = 0; i < 9; ++i) {
    output[i] = libfilter_hash_mix_none(input[i][0], entropy[i][0]);
    // TODO: should loading take care of this?
  }
  for (unsigned j = 1; j < 3; ++j) {
    for (unsigned i = 0; i < 9; ++i) {
      output[i] = libfilter_hash_mix_one(output[i], input[i][j], entropy[i][j]);
    }
  }
}

inline void libfilter_hash_ehc(const char input[7 * 3 * sizeof(libfilter_u64x2)],
                               const uint64_t raw_entropy[9][3],
                               libfilter_u64x2 output[3]) {
  libfilter_u64x2 scratch[9][3];
  libfilter_u64x2 tmpout[9];
  libfilter_hash_ehc_load(input, scratch);
  libfilter_hash_ehc_encode(scratch);
  libfilter_hash_ehc_hash(scratch, raw_entropy, tmpout);
  libfilter_hash_ehc_combine(tmpout, output);
}

 void libfilter_hash_tree(const char* data, size_t block_group_length,
                                libfilter_u64x2 stack[][8][3], int stack_lengths[],
                                const uint64_t* entropy) {
  const uint64_t(*entropy_matrix)[3] = (const uint64_t(*)[3])entropy;
  for (size_t k = 0; k < block_group_length; ++k) {
    int i = 0;
    while (stack_lengths[i] == 8) ++i;
    for (int j = i - 1; j >= 0; --j) {
      libfilter_hash_upper(stack[j], &entropy[9 * 3 + (8 - 1) * 3 * j],
                           stack[j + 1][stack_lengths[j + 1]]);
      stack_lengths[j] = 0;
      stack_lengths[j + 1] += 1;
    }

    libfilter_hash_ehc(&data[k * 7 * 3 * sizeof(libfilter_u64x2)], entropy_matrix,
                       stack[0][stack_lengths[0]]);
    stack_lengths[0] += 1;
  }
}

static uint64_t libfilter_hash_entropy_bytes =
    sizeof(uint64_t) * (9 * 3 + (8 - 1) * 3 * 10 + 2 * 8 * 3 * 10 + 2 * 7 * 3 + 3 - 1);

typedef struct {
  const uint64_t* seeds;
  libfilter_u64x2 accum[3];
} libfilter_hash_accum;

inline void libfilter_hash_greedy_stack(libfilter_hash_accum* here,
                                        const libfilter_u64x2 x[3]) {
  for (unsigned i = 0; i < 3; ++i) {
    here->accum[i] =
        libfilter_hash_mix(here->accum[i], x[i], libfilter_hash_load_block(here->seeds));
    here->seeds += sizeof(libfilter_u64x2) / sizeof(uint64_t);
  }
}

inline void libfilter_hash_greedy_input(libfilter_hash_accum* here, libfilter_u64x2 x) {
  for (unsigned i = 0; i < 3; ++i) {
    here->accum[i] = libfilter_hash_mix(
        here->accum[i], x,
        libfilter_hash_load_block(
            &here->seeds[i * sizeof(libfilter_u64x2) / sizeof(uint64_t)]));
  }
  // Toeplitz
  here->seeds += sizeof(libfilter_u64x2) / sizeof(uint64_t);
}

inline void libfilter_hash_greedy(const libfilter_hash_accum* here, uint64_t output[3]) {
  for (unsigned i = 0; i < 3; ++i) {
    output[i] = libfilter_hash_sum(here->accum[i]);
  }
}

inline void libfilter_hash_finalize(const libfilter_u64x2 stack[][8][3],
                                    const int stack_lengths[], const char* char_input,
                                    size_t char_length, const uint64_t* entropy,
                                    uint64_t output[3]) {
  libfilter_hash_accum b;
  b.seeds = entropy;
  memset(b.accum, 0, sizeof(b.accum));
  for (int j = 0; stack_lengths[j] > 0; ++j) {
    for (int k = 0; k < stack_lengths[j]; k += 1) {
      libfilter_hash_greedy_stack(&b, stack[j][k]);
    }
  }

  size_t i = 0;
  for (; i + sizeof(libfilter_u64x2) <= char_length; i += sizeof(libfilter_u64x2)) {
    libfilter_hash_greedy_input(&b, libfilter_hash_load_block(&char_input[i]));
  }

  if (1) {
    libfilter_u64x2 extra = {};
    memcpy(&extra, &char_input[i], char_length - i);
    libfilter_hash_greedy_input(&b, extra);
  } else {
    libfilter_u64x2 extra;
    char* extra_char = (char*)(&extra);
    for (unsigned j = 0; j < sizeof(libfilter_u64x2); ++j) {
      if (j < char_length - i) {
        extra_char[j] = char_input[i + j];
      } else {
        extra_char[j] = 0;
      }
    }
    libfilter_hash_greedy_input(&b, extra);
  }
  libfilter_hash_greedy(&b, output);
}

inline uint64_t libfilter_hash_tabulate_one(uint64_t input,
                                            const uint64_t entropy[256 * 8]) {
  const uint64_t(*table)[256] = (const uint64_t(*)[256])(entropy);
  uint64_t result = 0;
  for (unsigned i = 0; i < 8; ++i) {
    uint8_t index = input >> (i * CHAR_BIT);
    result ^= table[i][index];
  }
  return result;
}

void libfilter_hash_au(
    const uint64_t entropy[libfilter_hash_entropy_bytes / sizeof(uint64_t)],
    const char* char_input, size_t length, uint64_t output[3]) {
  libfilter_u64x2 stack[10][8][3];
  int stack_lengths[10] = {};
  size_t wide_length = length / sizeof(libfilter_u64x2) / (7 * 3);

  libfilter_hash_tree(char_input, wide_length, stack, stack_lengths, entropy);
  entropy += 9 * 3 + 3 * (8 - 1) * 10;

  uint64_t used_chars = wide_length * sizeof(libfilter_u64x2) * (7 * 3);
  char_input += used_chars;

  libfilter_hash_finalize(stack, stack_lengths, char_input, length - used_chars, entropy,
                          output);
}

uint64_t libfilter_hash_tabulate(const uint64_t* entropy, const char* char_input,
                                 size_t length) {
  const uint64_t(*table)[256] = (const uint64_t(*)[256])(entropy);
  entropy += 3 * 256;
  uint64_t output[3];
  libfilter_hash_au(entropy, char_input, length, output);
  uint64_t result = libfilter_hash_tabulate_one(length, &table[0][0]);
  for (int i = 0; i < 3; ++i) {
    result ^= libfilter_hash_tabulate_one(output[i], &table[8 * (i + 1)][0]);
  }
  return result;
}
