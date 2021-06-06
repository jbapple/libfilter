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

// Returns a single "insert_nanos" statistic, a single "fpp" statistic, and `reps`
// "find_nanos" statistics. FILTER_TYPE must support CreateWithBytes(), InsertHash(), and
// FindHash().
//
// Also prints each statistic to stdout as soon as it is generated.
template <typename FILTER_TYPE>
vector<Sample> Bench(uint64_t reps, uint64_t bytes, const vector<uint64_t>& to_insert,
                     const vector<uint64_t>& to_find) {
  Sample base;
  base.filter_name = FILTER_TYPE::Name();
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

  base.sample_type = "size";
  base.payload = filter.SizeInBytes();
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
    // do something nearly nilpotent that the compiler can't figure out is a no-op
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
  for (unsigned i = 0; i < (1000 * 1000 * 32); ++i) {
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
vector<Sample> TupleMultiBench(uint64_t ndv, uint64_t reps, uint64_t bytes) {
  return MultiBenchHelp<TUPLE>::Delegate(ndv, reps, bytes);
}


int main(int argc, char** argv) {
  if (argc < 7) {
  err:
    cerr << "one optional flag (--print_header) and three required flags: --ndv, --reps, "
            "--bytes\n";
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

  using TupleType = tuple<BlockFilter, ElasticFilter>;

  if (print_header) cout << Sample::kHeader() << endl;
  for (unsigned i = 0; i < reps; ++i) {
    TupleMultiBench<TupleType>(ndv, reps, bytes);
  }
}
