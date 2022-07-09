#include "filter/static.h"

#include <stdio.h>

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

typedef struct { uint64_t state;  uint64_t inc; } pcg32_random_t;

uint32_t pcg32_random_r(pcg32_random_t* rng)
{
    uint64_t oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    uint32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

int main() {
  pcg32_random_t seed;
  seed.inc = 0xefc0fadcd6224213;
  seed.state = 0x99047d494c2fc45d;
  for (size_t size = 10; size < 1 * 1000 * 1000; size = 1 + size * 2) {
    uint64_t* hashes = malloc(size * sizeof(uint64_t));
    for (unsigned i = 0; i < size; ++i) {
      hashes[i] = pcg32_random_r(&seed);
      hashes[i] = hashes[i] << 32;
      hashes[i] |= pcg32_random_r(&seed);
    }
    libfilter_static filter = libfilter_static_construct(size, hashes);
    printf("%zu\t%f\n", size, 1.0 * filter.length_ / size);
    fflush(stdout);
    for (unsigned i = 0; i < size; ++i) {
      assert(libfilter_static_find_hash(filter, hashes[i]));
    }
    libfilter_static_destruct(filter);
  }
}
