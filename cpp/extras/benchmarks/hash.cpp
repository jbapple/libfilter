// Benchmarks number of hash collisions when hashing a full filter

#include <limits.h>          // for CHAR_BIT
#include <cstdint>           // for uint64_t
#include <iostream>          // for operator<<, endl, basic_ostream, basi...
#include <unordered_set>

using namespace std;

#include "filter/block.hpp"  // for BlockFilter, ScalarBlockFilter, Scala...
#include "util.hpp"          // for Rand

using namespace filter;

template<typename Filter>
uint64_t experimental_hash_collisions(double ndv, double bytes) {
  Filter f = Filter::CreateWithBytes(bytes);
  Rand r;
  uint64_t result = 0;
  unordered_set<uint32_t> hashes;
  vector<uint64_t> entropy(libfilter_hash_tabulate_entropy_bytes / sizeof(uint64_t));

  for (unsigned i = 0; i < entropy.size(); ++i) {
    entropy[i] = r();
  }


  hashes.insert(f.SaltedHash(entropy.data()));
  while (ndv --> 0) {
    const auto it = r();
    auto hashed = f.SaltedHash(entropy.data());
    // cout << hashed << endl;
    result += !hashes.insert(hashed).second;
    f.InsertHash(it);
    if (0 == (hashes.size() & (hashes.size() - 1))) {
      Filter g = f;
      if (g.SaltedHash(entropy.data()) != f.SaltedHash(entropy.data())) exit(1);
    }
  }
  return result;
}

int main() {
  const double ndv = 1e4;
  const double m_over_n = 8 / log(2);
  const double bytes = m_over_n * ndv / CHAR_BIT;

  cout << BlockFilter::Name() << "\t\t"
       << experimental_hash_collisions<BlockFilter>(ndv, bytes) / ndv << endl;
}
