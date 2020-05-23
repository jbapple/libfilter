// Benchmarks predicted vs. experimental false positive probabilities

#include <limits.h>          // for CHAR_BIT
#include <cstdint>           // for uint64_t
#include <iostream>          // for operator<<, endl, basic_ostream, basi...

using namespace std;

#include "filter/block.hpp"  // for BlockFilter, ScalarBlockFilter, Scala...
#include "util.hpp"          // for Rand

using namespace filter;

template<typename Filter>
double experimental_fpp(double ndv, double bytes) {
  Filter f = Filter::CreateWithBytes(bytes);
  Rand r;
  while (ndv --> 0) {
    f.InsertHash(r());
  }
  double found = 0;
  uint64_t sample_size = 123456789;
  for(uint64_t i = 0; i < sample_size; ++i) {
    found += f.FindHash(r());
  }
  return found / sample_size;
}

int main() {
  const double ndv = 1e6;
  const double m_over_n = 8 / log(2);
  const double bytes = m_over_n * ndv / CHAR_BIT;

  cout << "Bloom would be:\t\t" << exp(-m_over_n * log(2) * log(2)) << endl;
  cout << "Expected fpp for block:\t" << BlockFilter::FalsePositiveProbability(ndv, bytes) << endl;
  cout << BlockFilter::Name() << "\t\t" << experimental_fpp<BlockFilter>(ndv, bytes)
       << endl;
  cout << ScalarBlockFilter::Name() << "\t"
       << experimental_fpp<ScalarBlockFilter>(ndv, bytes) << endl;
}
