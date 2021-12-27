#include <cassert>
#include <cstdint>

#include "filter/block.hpp"
#include "filter/minimal-taffy-cuckoo.hpp"
#include "filter/taffy-block.hpp"
#include "filter/taffy-cuckoo.hpp"
#if defined(__x86_64)
#include "filter/taffy-vector-quotient.hpp"
#endif

using namespace std;

int main() {
  const unsigned ndv = 1000000;
  const double fpp = 0.0065;
  const uint64_t hash = 0xfeedbadbee52b055;
  auto filter = filter::BlockFilter::CreateWithNdvFpp(ndv, fpp);
  filter.InsertHash(hash);
  assert(filter.FindHash(hash));
}
