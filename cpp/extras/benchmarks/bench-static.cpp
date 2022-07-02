// This is a benchmark of insert time, find time, (both in nanoseconds) and false positive
// probability. The results are printed to stdout.
//
// The output is CSV. Each line has the form
//
// filter_name, ndv, bytes, sample_type, payload
//
// The sample_type can be "insert_nanos", "find_nanos", or "fpp".

#include <algorithm>
#include <chrono>    // for nanoseconds, duration, duration_cast
#include <cstdint>   // for uint64_t
#include <iostream>  // for operator<<, basic_ostream, endl, istr...
#include <sstream>   // for basic_istringstream
#include <string>    // for string, operator<<, operator==
#include <tuple>     // for tuple
#include <utility>
#include <vector>  // for vector, allocator

#include "filter/static.hpp"
#include "util.hpp"  // for Rand

using namespace filter;

using namespace std;

// TODO: since we're printing out stats directly, there is no reason to return any values
// from the benchmarking functions.

// A single statistic. sample_type must be "insert_nanos", "find_missing_nanos",
// "find_present_nanos", "to_fin_base", "to_ins_base", or "fpp".
//
// "to_fin_base" is how long it takes just to iterate over the absent elements being
// found. This can be subtracted from find_missing_nanos to remove the baseline cost and
// focus on the find cost only.
//
// "to_ins_base" is the same, but for the present elements.
struct Sample {
  string filter_name = "", sample_type = "";
  uint64_t ndv_start = 0;
  uint64_t ndv_finish = 0;
  uint64_t bytes = 0;
  double payload = 0.0;

  static const char* kHeader() {
    static const char result[] = "filter_name,ndv_start,ndv_finish,bytes,sample_type,payload";
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
    o << ndv_start << "," << ndv_finish << "," << bytes << ",";
    o << EscapedName(sample_type) << ",";
    o << payload;
    return o.str();
  }
};

// Does the actual benchmarking work. Repeats `reps` times, samples grow by
// `growth_factor`.
//
// TODO: does all the printing. Why bother returning a value at all?
template <typename FILTER_TYPE>
vector<Sample> BenchHelp(uint64_t reps, double growth_factor, vector<uint64_t>& to_insert,
                         const vector<uint64_t>& to_find) {
  Sample base;
  base.filter_name = FILTER_TYPE::Name();

  vector<Sample> result;

  chrono::steady_clock s;

  // insert_nanos:
  double last = 0, next = 1;
  Rand r;
  while (last < to_insert.size()) {
    const auto start = s.now();
    uint64_t i = min(to_insert.size(), static_cast<size_t>(next));
    FILTER_TYPE filter(i, to_insert.data());
    const auto finish = s.now();
    base.ndv_start = last;
    base.ndv_finish = next;

    if (base.ndv_finish > base.ndv_start) {
      base.bytes = filter.SizeInBytes();
      const auto insert_time = static_cast<std::chrono::duration<double>>(finish - start);
      base.sample_type = "insert_nanos";
      // TODO: shouldn't the denominator be min(to_insert.size(),
      // static_cast<uint64_t>(next)) - last?
      base.payload = 1.0 *
                     chrono::duration_cast<chrono::nanoseconds>(insert_time).count() /
                     min(to_insert.size(), static_cast<size_t>(next - last));
      result.push_back(base);
      cout << base.CSV() << endl;

      uint64_t found = 0;
      // Just something to force computation so the optimizer doesn't fully elide a loop
      uint64_t dummy = 0;
      uint64_t found_monotonic = 0;
      for (unsigned j = 0; j < reps; ++j) {
        const auto start = s.now();
        for (unsigned i = 0; i < 1000 * 1000; ++i) {
          found += filter.FindHash(to_find[r() % static_cast<uint64_t>(next)]);
        }
        const auto finish = s.now();
        for (unsigned i = 0; i < 1000 * 1000; ++i) {
          dummy += to_find[r() % static_cast<uint64_t>(next)];
        }
        const auto finality = s.now();
        auto find_time = static_cast<std::chrono::duration<double>>(finish - start);
        base.sample_type = "find_missing_nanos";
        base.payload = 1.0 *
                       chrono::duration_cast<chrono::nanoseconds>(find_time).count() /
                       1000 / 1000;
        result.push_back(base);
        cout << base.CSV() << endl;

        find_time = static_cast<std::chrono::duration<double>>(finality - finish);
        base.sample_type = "to_fin_base";
        base.payload = 1.0 *
                       chrono::duration_cast<chrono::nanoseconds>(find_time).count() /
                       1000 / 1000;
        result.push_back(base);
        cout << base.CSV() << endl;
        for (unsigned i = 0; i < 1000 * 1000; ++i) {
          found_monotonic += filter.FindHash(to_find[i]);
        }
      }

      base.sample_type = "fpp";
      base.payload = 1.0 * found_monotonic / 1000 / 1000 / reps;
      result.push_back(base);
      cout << base.CSV() << endl;
      for (unsigned j = 0; j < reps; ++j) {
        const auto start = s.now();
        for (uint64_t i = 0; i < 1000 * 1000; ++i) {
          found += filter.FindHash(to_insert[r() % static_cast<uint64_t>(next)]);
        }
        const auto finish = s.now();
        for (uint64_t i = 0; i < 1000 * 1000; ++i) {
          found += to_insert[r() % static_cast<uint64_t>(next)];
        }
        const auto finality = s.now();
        auto find_time = static_cast<std::chrono::duration<double>>(finish - start);
        base.sample_type = "find_present_nanos";
        base.payload = 1.0 *
                       chrono::duration_cast<chrono::nanoseconds>(find_time).count() /
                       1000 / 1000;
        result.push_back(base);
        cout << base.CSV() << endl;

        find_time = static_cast<std::chrono::duration<double>>(finality - finish);
        base.sample_type = "to_ins_base";
        base.payload = 1.0 *
                       chrono::duration_cast<chrono::nanoseconds>(find_time).count() /
                       1000 / 1000;
        result.push_back(base);
        cout << base.CSV() << endl;
      }
      // Force the FindHash value to be calculated:
      if (found & dummy & found_monotonic) {
        // do something nearly nilpotent that the compiler can't figure out is a no-op
        result.push_back(base);
        result.pop_back();
      }
    }
    double tmp = last;
    last = next;
    next = (tmp * growth_factor) + 1;
  }

  return result;
}

void Samples(uint64_t ndv, vector<uint64_t>& to_insert, vector<uint64_t>& to_find) {
  Rand r;
  for (unsigned i = 0; i < ndv; ++i) {
    to_insert.push_back(r());
  }
  for (unsigned i = 0; i < std::max(ndv, static_cast<uint64_t>(1000 * 1000)); ++i) {
    to_find.push_back(r());
  }
}

int main(int argc, char** argv) {
  if (argc < 5) {
  err:
    cerr << "one optional flag (--print_header) and two required flags: --ndv, --reps\n";
    return 1;
  }
  uint64_t ndv = 0, reps = 0;
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
    } else if (argv[i] == string("--print_header")) {
      print_header = true;
    } else {
      goto err;
    }
  }
  if (reps == 0 or ndv == 0) goto err;

  vector<uint64_t> to_insert;
  vector<uint64_t> to_find;
  Samples(ndv, to_insert, to_find);

  if (print_header) cout << Sample::kHeader() << endl;
  for (unsigned i = 0; i < reps; ++i) {
    BenchHelp<StaticFilter>(reps, 2.0, to_insert, to_find);
  }
}
