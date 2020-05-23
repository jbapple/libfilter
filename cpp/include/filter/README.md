This directory contains block.hpp, which can be installed in a system
include/ folder (like /usr/local/include/). It depends on the headers
and libraries in in the `C` part of this project, so to install,
please see the instructions in the root directory of this project.

When compiling, you'll need to link to libfilter.so, via '-lfilter' in
GCC or Clang.

The interface is essentially:

```C++
class BlockFilter {
  static uint64_t MinSpaceNeeded(uint64_t ndv, double fpp);
  static BlockFilter CreateWithBytes(uint64_t bytes);
  static BlockFilter CreateWithNdvFpp(uint64_t ndv, double fpp);
  void InsertHash(uint64_t hash);
  bool FindHash(uint64_t hash) const;

  uint64_t SizeInBytes() const;

  BlockFilter(BlockFilter &&);
  BlockFilter(const BlockFilter &);
  BlockFilter& operator=(const BlockFilter &);

  static double FalsePositiveProbability(uint64_t ndv, uint64_t bytes);
  static uint64_t MaxCapacity(uint64_t bytes, double fpp);
};
```
