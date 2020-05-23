// Prints CSV containing false positive percent and then the wasted space for each of
// several filter types. Spaace wasted is the percent of space used over log2(1 / fpp),
// which is the static minimum.

#include <cstdint>
#include <iostream>
#include <limits>

using namespace std;

extern "C" {
#include "util.h"
}

int main() {
  uint64_t ndv = 1'000'000;

  // Print headers
  cout << "v,";
  for (int arity : {4, 8, 16}) {
    for (int word : {32, 64}) {
      if (arity * word > 512) continue;
      cout << arity << "x" << word << ",";
      cout << arity << "x" << word << "-s1" << ",";
    }
  }
  cout << "bloom,shingle cuckoo (93%),rsqf (90%)\n";

  for (double fpp = 0.1; fpp >= 1.0 / 1'000'000; fpp = fpp / pow(10, 0.25)) {
    // The static minimum bits needed per distinct value
    const double min_bits_per = log2(1 / fpp);
    cout << 100 * fpp << ",";
    for (int arity : {4, 8, 16}) {
      for (int word : {32, 64}) {
        if (arity * word > 512) continue;
        double bytes_needed = libfilter_block_bytes_needed(ndv, fpp, word, arity, word);
        double bits_needed = CHAR_BIT * bytes_needed;
        double wasted_space = 100 * ((bits_needed / ndv) - min_bits_per) / min_bits_per;
        cout << wasted_space << ",";
        bytes_needed = libfilter_generic_bytes_needed(ndv, fpp, word, arity, word,
                                                      libfilter_block_shingle_1_fpp);
        bits_needed = CHAR_BIT * bytes_needed;
        wasted_space = 100 * ((bits_needed / ndv) - min_bits_per) / min_bits_per;
        cout << wasted_space << ",";

      }
    }

    // min_m is the number of bits needed per distinct value in the traditional Bloom
    // filter; right_i is the number of bits to set in order to get that minimum.
    double min_m = numeric_limits<double>::infinity();
    // int right_i = 0;
    for (int i = 1; i < 100; ++i) {
      double new_m = -i / log(1 - pow(fpp, 1.0 / i));
      if (new_m < min_m) {
        // right_i = i;
        min_m = new_m;
      }
    }
    cout << (100.0 * (min_m / min_bits_per) - 100) << ",";
    cout << 100 * (((log2(1 / fpp) + 2) / 0.93 - min_bits_per) / min_bits_per) << ",";
    cout << 100 * (((log2(1 / fpp) + 2.125) / 0.90 - min_bits_per) / min_bits_per);
    cout << endl;
  }
}
