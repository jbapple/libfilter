![](https://github.com/jbapple/libfilter/workflows/c-cpp/badge.svg?branch=master)

This repository provides implementations of blocked Bloom filters for
C, C++, and Java. These filters use slightly more space than
traditional Bloom filters but are much faster.

Example usage in C:

```C
#include <filter/block.h>

unsigned ndv = 1000000;
double fpp = 0.0065;
uint64_t hash = 0xfeedbadbee52b055;

libfilter_block filter;

unsigned bytes = libfilter_block_bytes_needed(ndv, fpp);
libfilter_block_init(bytes, &filter);
libfilter_block_add_hash(hash, &filter);
assert(libfilter_block_find_hash(hash, &filter));
libfilter_block_destruct(&filter);
```

in C++:

```C++
#include <filter/block.hpp>

unsigned ndv = 1000000;
double fpp = 0.0065;
uint64_t hash = 0xfeedbadbee52b055;

auto filter = filter::BlockFilter::CreateWithNdvFpp(ndv, fpp);
filter.InsertHash(hash);
assert(filter.FindHash(hash));
```

in Java

```Java
import com.github.jbapple.libfilter.BlockFilter;

int ndv = 1000000;
double fpp = 0.0065;
long hash = (((long) 0xfeedbadb) << 32) | (long) 0xee52b055;

BlockFilter = BlockFilter.CreateWithNdvFpp(ndv, fpp);
filter.AddHash64(hash);
assert filter.FindHash64(hash)
```
To install:

```shell
make
# probably needs a sudo:
make install
```

Block filters are most space-efficient at around 11.5 bits per
distinct value, which produces a false positive probability of
0.65%. A traditional Bloom filter would only need 10.5 bits per
distinct value to achieve that same false positive probability, while
a cuckoo filter would need 10.7 bits per distinct value.
