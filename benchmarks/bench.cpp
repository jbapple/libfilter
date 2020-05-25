// This is a benchmark of insert time, find time, (both in nanoseconds) and false positive
// probability. The results are printed to stdout.
//
// The output is CSV. Each line has the form
//
// filter_name, ndv, bytes, sample_type, payload
//
// The sample_type can be "insert_nanos", "find_nanos", or "fpp".

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <tuple>
#include <vector>

#include "simd-block.hpp"
#include "util.hpp"

using namespace filter;

using namespace std;

// A single statistic. sample_type must be "insert_nanos", "find_nanos", or "fpp".
struct Sample {
  string filter_name = "", sample_type = "";
  uint64_t ndv = 0;
  uint64_t bytes = 0;
  double payload = 0.0;

  inline const static char kHeader[] = "filter_name,ndv,bytes,sample_type,payload";

  // Escape raq quotation marks in strings
  static string EscapedName(const string& x) {
    string result = "\"";
    for (char c : x) {
      result += c;
      if (c == '"') result += "\"";
    }
    result += "\"";
    return result;
  }

  string CSV() const {
    ostringstream o;
    o << EscapedName(filter_name) << ",";
    o << ndv << "," << bytes << ",";
    o << EscapedName(sample_type) << ",";
    o << payload;
    return o.str();
  }
};

// Returns a single "insert_nanos" statistic, a single "fpp" statistic, and `reps`
// "find_nanos" statistics. FILTER_TYPE must support CreateWithBytes(), InsertHash(), and
// FindHash().
//
// Also prints each statistic to stdout as soon as it is generated.
template <typename FILTER_TYPE>
vector<Sample> Bench(uint64_t reps, uint64_t bytes, const vector<uint64_t>& to_insert,
                     const vector<uint64_t>& to_find) {
  Sample base;
  base.filter_name = FILTER_TYPE::NAME;
  base.ndv = to_insert.size();
  base.bytes = bytes;

  vector<Sample> result;

  auto filter = FILTER_TYPE::CreateWithBytes(bytes);

  chrono::steady_clock s;

  // insert_nanos:
  const auto start = s.now();
  for (unsigned i = 0; i < to_insert.size(); ++i) {
    filter.InsertHash(to_insert[i]);
  }
  const auto finish = s.now();
  const auto insert_time = static_cast<std::chrono::duration<double>>(finish - start);
  base.sample_type = "insert_nanos";
  base.payload =
      1.0 * chrono::duration_cast<chrono::nanoseconds>(insert_time).count() / to_insert.size();
  result.push_back(base);
  cout << base.CSV() << endl;

  // fpp:
  uint64_t found = 0;
  for (unsigned i = 0; i < to_find.size(); ++i) {
    found += filter.FindHash(to_find[i]);
  }
  base.sample_type = "fpp";
  base.payload = 1.0 * found / to_find.size();
  result.push_back(base);
  cout << base.CSV() << endl;

  // `reps` find_nanos
  for (unsigned j = 0; j < reps; ++j) {
    const auto start = s.now();
    for (unsigned i = 0; i < to_find.size(); ++i) {
      found += filter.FindHash(to_find[i]);
    }
    const auto finish = s.now();
    const auto find_time = static_cast<std::chrono::duration<double>>(finish - start);
    base.sample_type = "find_nanos";
    base.payload = 1.0 * chrono::duration_cast<chrono::nanoseconds>(find_time).count() /
                   to_find.size();
    result.push_back(base);
    cout << base.CSV() << endl;
  }

  // Force the FindHash value to be calculated:
  if (found) {
    // do something nilpotent:
    result.push_back(base);
    result.pop_back();
  }

  return result;
}

// Calls Bench() on each member of a parameter pack
template <typename... FILTER_TYPES>
vector<Sample> MultiBench(uint64_t ndv, uint64_t reps, uint64_t bytes) {
  Rand r;
  vector<uint64_t> to_insert, to_find;
  for (unsigned i = 0; i < ndv; ++i) {
    to_insert.push_back(r());
  }
  for (unsigned i = 0; i < 1'000'000; ++i) {
    to_find.push_back(r());
  }

  vector<Sample> result;

  for (const auto& v : {Bench<FILTER_TYPES>(reps, bytes, to_insert, to_find)...}) {
    for (const auto& w : v) {
      result.push_back(w);
    }
  }

  return result;
}

// MultiBenchHelp and TupleMultiBench are an unfortunate construction needed to be able to
// call MultiBench with a specific set of parameters without inserting them into each
// call; i.e., giving a specific ordered set of types a name in a non-template context.

template <typename>
struct MultiBenchHelp {};

template <typename... FILTER_TYPES>
struct MultiBenchHelp<tuple<FILTER_TYPES...>> {
  static vector<Sample> Delegate(uint64_t ndv, uint64_t reps, uint64_t bytes) {
    return MultiBench<FILTER_TYPES...>(ndv, reps, bytes);
  }
};

template <typename TUPLE>
auto TupleMultiBench(uint64_t ndv, uint64_t reps, uint64_t bytes) {
  return MultiBenchHelp<TUPLE>::Delegate(ndv, reps, bytes);
}


int main(int argc, char** argv) {
  if (argc < 7) {
  err:
    cerr << "one optional option (--print_header) and three required options: --ndv, "
            "--reps, --bytes\n";
    return 1;
  }
  uint64_t ndv = 0, reps = 0, bytes = 0;
  bool print_header = false;
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == string("--ndv")) {
      ++i;
      auto s = istringstream(argv[i]);
      if (not(s >> ndv)) goto err;
      if (not s.eof()) goto err;
    } else if (argv[i] == string("--reps")) {
      ++i;
      auto s = istringstream(argv[i]);
      if (not(s >> reps)) goto err;
      if (not s.eof()) goto err;
    } else if (argv[i] == string("--bytes")) {
      ++i;
      auto s = istringstream(argv[i]);
      if (not(s >> bytes)) goto err;
      if (not s.eof()) goto err;
    } else if (argv[i] == string("--print_header")) {
      print_header = true;
    } else {
      goto err;
    }
  }
  if (ndv == 0 or reps == 0 or bytes == 0) goto err;

  // using TupleType =
  //     tuple<ScalarBlockFilter<5, 128>, BlockFilter<5, 128>,
  //           ScalarBlockFilter<5, 256>, BlockFilter<5, 256>,
  //           ScalarBlockFilter<5, 512>, BlockFilter<5, 512>,
  //           ScalarBlockFilter<6, 256>, BlockFilter<6, 256>,
  //           ScalarBlockFilter<6, 512>, BlockFilter<6, 512>,

  //           ScalarShingleBlockFilter<5, 128, 1>, ShingleBlockFilter<5, 128, 1>,
  //           ScalarShingleBlockFilter<5, 256, 1>, ShingleBlockFilter<5, 256, 1>,
  //           ScalarShingleBlockFilter<5, 512, 1>, ShingleBlockFilter<5, 512, 1>,
  //           ScalarShingleBlockFilter<6, 256, 1>, ShingleBlockFilter<6, 256, 1>,
  //           ScalarShingleBlockFilter<6, 512, 1>, ShingleBlockFilter<6, 512, 1>,

  //           ScalarShingleBlockFilter<5, 128, 2>, ShingleBlockFilter<5, 128, 2>,
  //           ScalarShingleBlockFilter<5, 256, 2>, ShingleBlockFilter<5, 256, 2>,
  //           ScalarShingleBlockFilter<5, 512, 2>, ShingleBlockFilter<5, 512, 2>,
  //           ScalarShingleBlockFilter<6, 256, 2>, ShingleBlockFilter<6, 256, 2>,
  //           ScalarShingleBlockFilter<6, 512, 2>, ShingleBlockFilter<6, 512, 2>,

  //           ScalarShingleBlockFilter<5, 128, 4>, ShingleBlockFilter<5, 128, 4>,
  //           ScalarShingleBlockFilter<5, 256, 4>, ShingleBlockFilter<5, 256, 4>,
  //           ScalarShingleBlockFilter<5, 512, 4>, ShingleBlockFilter<5, 512, 4>,
  //           ScalarShingleBlockFilter<6, 256, 4>, ShingleBlockFilter<6, 256, 4>,
  //           ScalarShingleBlockFilter<6, 512, 4>, ShingleBlockFilter<6, 512, 4>,

  //           ScalarShingleBlockFilter<5, 128, 8>, ShingleBlockFilter<5, 128, 8>,
  //           ScalarShingleBlockFilter<5, 256, 8>, ShingleBlockFilter<5, 256, 8>,
  //           ScalarShingleBlockFilter<5, 512, 8>, ShingleBlockFilter<5, 512, 8>,
  //           ScalarShingleBlockFilter<6, 256, 8>, ShingleBlockFilter<6, 256, 8>,
  //           ScalarShingleBlockFilter<6, 512, 8>, ShingleBlockFilter<6, 512, 8>,

  //           ScalarShingleBlockFilter<5, 128, 16>, ShingleBlockFilter<5, 128, 16>,
  //           ScalarShingleBlockFilter<5, 256, 16>, ShingleBlockFilter<5, 256, 16>,
  //           ScalarShingleBlockFilter<5, 512, 16>, ShingleBlockFilter<5, 512, 16>,
  //           ScalarShingleBlockFilter<6, 256, 16>, ShingleBlockFilter<6, 256, 16>,
  //           ScalarShingleBlockFilter<6, 512, 16>, ShingleBlockFilter<6, 512, 16>,

  //           ScalarShingleBlockFilter<5, 256, 32>, ShingleBlockFilter<5, 256, 32>,
  //           ScalarShingleBlockFilter<5, 512, 32>, ShingleBlockFilter<5, 512, 32>,
  //           ScalarShingleBlockFilter<6, 256, 32>, ShingleBlockFilter<6, 256, 32>,
  //           ScalarShingleBlockFilter<6, 512, 32>, ShingleBlockFilter<6, 512, 32>,

  //           ScalarShingleBlockFilter<5, 512, 64>, ShingleBlockFilter<5, 512, 64>,
  //           ScalarShingleBlockFilter<6, 512, 64>, ShingleBlockFilter<6, 512, 64>

  //           >;

  using TupleType = tuple<BlockFilter<8, 32>, BlockFilter<8, 64>,
                          ShingleBlockFilter<8, 32, 1>, ShingleBlockFilter<8, 64, 1>>;

  if (print_header) cout << Sample::kHeader << endl;
  for (unsigned i = 0; i < reps; ++i) {
    TupleMultiBench<TupleType>(ndv, reps, bytes);
  }
}
