This directory contains block.hpp and elastic.hpp, which can be
installed in a system include/ folder (like /usr/local/include/). They
depend on the headers and libraries in in the `C` part of this
project, so to install, please see the instructions in the root
directory of this project.

When compiling, you'll need to link to libfilter.so, via '-lfilter' in
GCC or Clang.

The interfaces are essentially (for `FilterType` in (`BlockFilter`, `ElasticFilter`)):

```C++
class FilterType {
  static FilterType CreateWithBytes(uint64_t bytes);
  void InsertHash(uint64_t hash);
  bool FindHash(uint64_t hash) const;

  uint64_t SizeInBytes() const;

  FilterType(FilterType &&);
  FilterType(const FilterType &);
  FilterType& operator=(const FilterType &);

};
```

Additionally, `BlockFilter` provides

```C++
class BlockFilter {
  static uint64_t MinSpaceNeeded(uint64_t ndv, double fpp);
  static BlockFilter CreateWithNdvFpp(uint64_t ndv, double fpp);
  static double FalsePositiveProbability(uint64_t ndv, uint64_t bytes);
  static uint64_t MaxCapacity(uint64_t bytes, double fpp);
};
```
