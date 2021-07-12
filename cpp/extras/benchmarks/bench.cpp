// This is a benchmark of insert time, find time, (both in nanoseconds) and false positive
// probability. The results are printed to stdout.
//
// The output is CSV. Each line has the form
//
// filter_name, ndv, bytes, sample_type, payload
//
// The sample_type can be "insert_nanos", "find_nanos", or "fpp".

#include <chrono>            // for nanoseconds, duration, duration_cast
#include <cstdint>           // for uint64_t
#include <iostream>          // for operator<<, basic_ostream, endl, istr...
#include <sstream>           // for basic_istringstream
#include <string>            // for string, operator<<, operator==
#include <tuple>             // for tuple
#include <vector>            // for vector, allocator

#include "filter/block.hpp"  // for BlockFilter, ScalarBlockFilter (ptr o...
#include "filter/elastic.hpp"
#include "filter/block-elastic.hpp"
#include "filter/minimal-plastic.hpp"
#include "util.hpp"          // for Rand

using namespace filter;

using namespace std;

// TODO: since we're printing out stats directly, there is no reason to return any values
// from the benchmarking functions.

// A single statistic. sample_type must be "insert_nanos", "find_nanos", or "fpp".
struct Sample {
  string filter_name = "", sample_type = "";
  uint64_t ndv = 0;
  uint64_t bytes = 0;
  double payload = 0.0;

  static const char* kHeader() {
    static const char result[] = "filter_name,ndv,bytes,sample_type,payload";
    return result;
  };

  // Escape quotation marks in strings
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


template <typename FILTER_TYPE>
vector<Sample> BenchHelp(uint64_t reps, const vector<uint64_t>& to_insert,
                         const vector<uint64_t>& to_find, FILTER_TYPE& filter) {
  Sample base;
  base.filter_name = FILTER_TYPE::Name();
  base.ndv = to_insert.size();
  // base.bytes = bytes;

  vector<Sample> result;

  chrono::steady_clock s;

  // insert_nanos:
  const auto start = s.now();
  for (unsigned i = 0; i < to_insert.size(); ++i) {
    filter.InsertHash(to_insert[i]);
  }
  base.bytes = filter.SizeInBytes();
  const auto finish = s.now();
  const auto insert_time = static_cast<std::chrono::duration<double>>(finish - start);
  base.sample_type = "insert_nanos";
  base.payload =
      1.0 * chrono::duration_cast<chrono::nanoseconds>(insert_time).count() / to_insert.size();
  result.push_back(base);
  cout << base.CSV() << endl;

  // base.sample_type = "size";
  // base.payload = filter.SizeInBytes();
  // result.push_back(base);
  // cout << base.CSV() << endl;

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
    // do something nearly nilpotent that the compiler can't figure out is a no-op
    result.push_back(base);
    result.pop_back();
  }

  return result;
}

// Returns a single "insert_nanos" statistic, a single "fpp" statistic, and `reps`
// "find_nanos" statistics. FILTER_TYPE must support CreateWithBytes(), InsertHash(), and
// FindHash().
//
// Also prints each statistic to stdout as soon as it is generated.
template <typename FILTER_TYPE>
vector<Sample> BenchWithBytes(uint64_t reps, uint64_t bytes,
                              const vector<uint64_t>& to_insert,
                              const vector<uint64_t>& to_find) {
  auto filter = FILTER_TYPE::CreateWithBytes(bytes);
  return BenchHelp(reps, to_insert, to_find, filter);
}

template <typename FILTER_TYPE>
vector<Sample> BenchWithNdvFpp(uint64_t reps, const vector<uint64_t>& to_insert,
                               const vector<uint64_t>& to_find, uint64_t ndv,
                               double fpp) {
  auto filter = FILTER_TYPE::CreateWithNdvFpp(ndv, fpp);
  return BenchHelp(reps, to_insert, to_find, filter);
}

template <typename FILTER_TYPE>
vector<Sample> BenchGrowWithNdvFpp(uint64_t reps, const vector<uint64_t>& to_insert,
                               const vector<uint64_t>& to_find, uint64_t ndv,
                               double fpp) {
  auto filter = FILTER_TYPE::CreateWithNdvFpp(32, fpp);
  return BenchHelp(reps, to_insert, to_find, filter);
}

// Calls Bench() on each member of a parameter pack
void Samples(uint64_t ndv, vector<uint64_t>& to_insert, vector<uint64_t>& to_find) {
  Rand r;
  for (unsigned i = 0; i < ndv; ++i) {
    to_insert.push_back(r());
  }
  for (unsigned i = 0; i < (1000 * 1000 * 32); ++i) {
    to_find.push_back(r());
  }
}

int main(int argc, char** argv) {
  if (argc < 7) {
  err:
    cerr << "one optional flag (--print_header) and four required flags: --ndv, --reps, "
            "--bytes, --fpp\n";
    return 1;
  }
  uint64_t ndv = 0, reps = 0, bytes = 0;
  double fpp = 0.0;
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
    } else if (argv[i] == string("--fpp")) {
      ++i;
      auto s = istringstream(argv[i]);
      if (not(s >> fpp)) goto err;
      if (not s.eof()) goto err;
    } else if (argv[i] == string("--print_header")) {
      print_header = true;
    } else {
      goto err;
    }
  }
  if (reps == 0 or ndv == 0 or reps == 0 or fpp == 0 or bytes == 0) goto err;

  vector<uint64_t> to_insert;
  vector<uint64_t> to_find;
  Samples(ndv, to_insert, to_find);

  if (print_header) cout << Sample::kHeader() << endl;
  for (unsigned i = 0; i < reps; ++i) {
    BenchWithBytes<MinimalPlasticFilter>(reps, bytes, to_insert, to_find);
    BenchWithBytes<ElasticFilter>(reps, bytes, to_insert, to_find);
    BenchGrowWithNdvFpp<BlockElasticFilter>(reps, to_insert, to_find, ndv, fpp);
    BenchWithNdvFpp<BlockFilter>(reps, to_insert, to_find, ndv, fpp);
    BenchWithBytes<BlockFilter>(reps, bytes, to_insert, to_find);

  }
  //using TupleType = tuple<BlockFilter, ElasticFilter, BlockElasticFilter>;

}
