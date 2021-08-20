This directory contains block.hpp taffy-block.hpp, taffy-cuckoohpp,
and minimal-taffy-cuckoo.hpp, which can be installed in a system
include/ folder (like /usr/local/include/). These headers depend on
the headers and libraries in in the `C` part of this project, so to
install, please see the instructions in the root directory of this
project.

When compiling and using block.hpp, you'll need to link to
libfilter.so, via '-lfilter' in GCC or Clang.

The interfaces are essentially (for `FilterType`

```C++
class FilterType {
  static FilterType CreateWithBytes(uint64_t bytes);
  static FilterType CreateWithNdvFpp(uint64_t ndv, double fpp);
  void InsertHash(uint64_t hash);
  bool FindHash(uint64_t hash) const;

  uint64_t SizeInBytes() const;

  FilterType(FilterType &&);
  FilterType(const FilterType &);
  FilterType& operator=(const FilterType &);
};
```

Generally, only one `CreateWith` method is available.
