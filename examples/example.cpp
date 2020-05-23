#include "simd-block.hpp"

#include <iostream>
#include <vector>

#include "util.hpp"

using namespace std;

int main() {
  const auto ndv = 1000 * 1000;
  const auto bits = 8 * ndv;
  cout << libfilter_block_fpp(ndv, bits / CHAR_BIT, 32, 8, 32) << endl;
  cout << libfilter_block_shingle_1_fpp(ndv, bits / CHAR_BIT, 32, 8, 0.000) << endl;

  auto f1 = filter::ShingleBlockFilter<8, 32, 4>::CreateWithBytes(bits / CHAR_BIT);
  auto f2 = filter::ShingleBlockFilter<8, 32, 1>::CreateWithBytes(bits / CHAR_BIT);
  Rand gen;
  for (auto i = 0; i < ndv; ++i) {
    auto h = gen();
    f1.InsertHash(h);
    f2.InsertHash(h);
  }
  double found1 = 0, found2 = 0;
  // double last = numeric_limits<double>::infinity();
  for (unsigned i = 1; i != 0; ++i) {
    auto h = gen();
    found1 += f1.FindHash(h);
    found2 += f2.FindHash(h);
    if (found1 > 0 && i > ndv && 0 == (i & (i - 1))) {
      cout << found1 / i << '\t' << found2 / i << endl;
      cout << 100.0 * i / (i - i - 1) << "%\t";
      cout << 100 * (1.0 - found2 / found1) << "%" << endl;
    }
  }
  cout << 100.0 << "%\t" << 100 * (1.0 - found2 / found1) << "%" << endl;
}
