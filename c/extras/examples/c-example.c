#include <stdint.h>
#include "filter/block.h"
// Once installed, one might use:
// #include <filter/block.h>

#include <assert.h>

int main() {
  const unsigned ndv = 1000000;
  const double fpp = 0.0065;
  const uint64_t hash = 0xfeedbadbee52b055;
  const unsigned bytes = libfilter_block_bytes_needed(ndv, fpp);
  libfilter_block filter;
  libfilter_block_init(bytes, &filter);
  libfilter_block_add_hash(hash, &filter);
  assert(libfilter_block_find_hash(hash, &filter));
  libfilter_block_destruct(&filter);
}
