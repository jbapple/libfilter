This repository provides an implementation of blocked Bloom filters
for C and C++, which use slightly more space than traditional Bloom
filters but are much faster.

Example usage:

```C
SimdBlockBloomFilter filter;
libfilter_block_init(123456 /* space in bytes */, 256 / CHAR_BIT, &filter);
const uint64_t hash = 0xfeedbadbee52b055;
libfilter_block_5_256_add_hash(hash, &filter);
assert(libfilter_block_5_256_find_hash(hash, &filter));
libfilter_block_destruct(256 / CHAR_BIT, &filter);
```
