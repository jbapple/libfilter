#include "filter/block.hpp"

#include <cstdint>
#include <cassert>

using namespace std;

int main() {
  const unsigned ndv = 1000000;
  const double fpp = 0.0065;
  const uint64_t hash = 0xfeedbadbee52b055;
  auto filter = filter::BlockFilter::CreateWithNdvFpp(ndv, fpp);
  filter.InsertHash(hash);
  assert(filter.FindHash(hash));
}
