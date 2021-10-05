#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#include "filter/util.hpp"

struct Cuckoo32 {
  class Hash32 {
    uint64_t mult, add;

   public:
    Hash32() {
      std::random_device raw("/dev/urandom");

      mult = raw();
      mult = mult << 32;
      mult |= raw();

      add = raw();
      add = add << 32;
      add |= raw();
    }

    uint32_t operator()(uint32_t x) const {
      return (mult * static_cast<uint64_t>(x) + add) >> 32;
    }
  };

  filter::detail::PcgRandom r{2};
  Hash32 hashes[2];
  std::vector<uint32_t> stash;
  bool has_zero = false;
  using Bucket = uint32_t[4];
  Bucket* sides[2];
  uint32_t occupancy = 0;
  int log_side_size;

 public:
  Cuckoo32() : stash(), log_side_size(0) {
    for (int i = 0; i < 2; ++i) {
      sides[i] = new Bucket[1ull << log_side_size]();
    }
  }

  ~Cuckoo32() {
    for (int i = 0; i < 2; ++i) delete[] sides[i];
  }

private:
  uint32_t InsertOrEvict(Bucket b, uint32_t key) {
    if (0 == key) {
      has_zero = true;
      return 0;
    }
    for (int i = 0; i < 4; ++i) {
      if (0 == b[i]) {
        b[i] = key;
        ++occupancy;
        return 0;
      }
    }
    using std::swap;
    swap(b[r.Get()], key);
    return key;
  }

 public:
  uint64_t Capacity() const { return 8 << log_side_size; }

  void Insert(uint32_t key) {
    if (0 == key) {
      has_zero = true;
      return;
    }
    if (occupancy > 0.9 * Capacity() || stash.size() > 8) Upsize();
    int ttl = 64;
    int i = 0;
    while (true) {
      uint64_t bucket_index = hashes[i](key) & ((1ull << log_side_size) - 1);
      // std::cout << std::hex << sides[i] << ' ' << std::dec << bucket_index << ' '
      //           << log_side_size << std::endl;
      key = InsertOrEvict(sides[i][bucket_index], key);
      if (0 == key) return;
      if (ttl <= 0) {
        stash.push_back(key);
        return;
      }
      i = 1 - i;
      --ttl;
    }
  }

 private:
  bool Find(const Bucket b, uint32_t k) const {
    for (int i = 0; i < 4; ++i) {
      if (b[i] == k) return true;
    }
    return false;
  }

 public:
  bool Find(uint32_t key) const {
    if (0 == key) return has_zero;
    for (auto v : stash) {
      if (v == key) return true;
    }
    for (int i = 0; i < 2; ++i) {
      if (Find(sides[i][hashes[i](key) & ((1ull << log_side_size) - 1)], key)) {
        return true;
      }
    }
    return false;
  }

 private:
  void Upsize() {
    Bucket* old_sides[2] = {sides[0], sides[1]};
    std::vector<uint32_t> old_stash;
    using std::swap;
    swap(old_stash, stash);
    occupancy = 0;
    log_side_size += 1;
    for (int j = 0; j < 2; ++j) {
      sides[j] = new Bucket[1ull << log_side_size]();
    }
    for (auto v : old_stash) {
      Insert(v);  // TODO no Upsize during Upsize
    }
    for (int j = 0; j < 2; ++j) {
      for (uint64_t i = 0; i < (1ull << (log_side_size - 1)); ++i) {
        for (int k = 0; k < 4; ++k) {
          Insert(old_sides[j][i][k]);
        }
      }
      delete[] old_sides[j];
    }
  }
};
