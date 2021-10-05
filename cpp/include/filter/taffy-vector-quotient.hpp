#include <immintrin.h>

#include <cstdint>
#include <string>

namespace filter {

thread_local const constexpr int kHeadSize = 9;
thread_local const constexpr int kTailSize = 5;
static_assert(kHeadSize + kTailSize == 14, "kHeadSize + kTailSize == 15");

struct TaffyVectorQuotientFilter {
  uint64_t occupancy = 0;
  static std::string Name() {
    thread_local static const std::string result = "TVQF";
    return result;
  }

  uint64_t SizeInBytes() const { return sizeof(Line) << log_size; }

  static TaffyVectorQuotientFilter CreateWithBytes(uint64_t) {
    constexpr uint64_t entropy[8] = {
        0xb15dbfc96a694e83, 0x52837326421249c7, 0x50a38b0aec7c4baa, 0x5e71de01da7842e0,
        0xc675b743f7c74fae, 0x42d64f9d750b46b5, 0xa6fafa9aac5d4c8b, 0xf394f37b5e4c4972};
    return TaffyVectorQuotientFilter(0, entropy);
  }

  struct Line {
    uint64_t metadata = 0xffffffff;

    struct Entry {
      uint64_t lean : 1;
      uint64_t fingerprint : kHeadSize;
      uint64_t tail : kTailSize + 1;
    } __attribute__((packed));

    static_assert(sizeof(Entry) == 2, "Entry size");

    Entry data[28];

    __attribute__((always_inline)) bool Insert(uint64_t lean, uint64_t quotient,
                                               uint64_t fingerprint, uint64_t tail) {
      if (_lzcnt_u64(metadata) == 4) {
        //std::cout << std::hex << metadata << std::endl;
        return false;
      }
      int nth_one = _tzcnt_u64(_pdep_u64(1 << quotient, metadata));
      assert(nth_one < 60);
      uint64_t new_metadata =
          (metadata & ((1ull << nth_one) - 1)) | (metadata >> nth_one << (nth_one + 1));

      // std::cout << "meta\n"
      //           << std::hex << metadata << std::endl
      //           << std::hex << new_metadata << std::endl;
      assert(_lzcnt_u64(metadata) == 1 + _lzcnt_u64(new_metadata));
      assert(_mm_popcnt_u64(metadata) == 32);
      assert(_mm_popcnt_u64(new_metadata) == 32);

      /*
      int floc = loc - quotient;
      floc = std::min(floc, 26);      // TODO: junk
      assert(0 <= floc);
      assert(floc < 27);
      */
      // std::cout << "nth\n"
      //           << std::dec << nth_one << std::endl
      //           << std::dec << quotient << std::endl;
      auto payload_index = nth_one - quotient;
      assert(payload_index >= 0);
      assert(payload_index < 28);
      memmove(&data[payload_index + 1], &data[payload_index], sizeof(Entry) * (28 - payload_index - 1));

      data[payload_index] = {lean, fingerprint, tail};
      metadata = new_metadata;
      return true;
    }

    __attribute__((always_inline)) bool Find(uint64_t lean, uint64_t quotient,
                                             uint64_t fingerprint, uint64_t tail) {
      int nth_one = _tzcnt_u64(_pdep_u64(1 << quotient, metadata));
      int begin = quotient ? (1 + _tzcnt_u64(_pdep_u64(1 << (quotient - 1), metadata))) : 0;
      for (int i = begin; i < nth_one; ++i) {
        if (data[i].lean == lean && data[i].fingerprint == fingerprint &&
            detail::IsPrefixOf(data[i].tail, tail)) {
          return true;
        }
      }
      return false;
    }

    __attribute__((always_inline)) int Population() const {
      return 64 - _lzcnt_u64(metadata) - 32;
    }
  } __attribute__((packed));

  static_assert(sizeof(Line) == 64, "Line size");

  int log_size;
  Line* lines;
  detail::Feistel f[2];

  TaffyVectorQuotientFilter(int log_size, const uint64_t entropy[8])
      : log_size(log_size),
        lines(new Line[1ull << log_size]()),
        f{entropy, entropy + 4} {}

  ~TaffyVectorQuotientFilter() { delete[] lines; }

  friend void swap(TaffyVectorQuotientFilter&, TaffyVectorQuotientFilter&);

  __attribute__((always_inline)) bool InsertNeedsNoUpsize(uint64_t raw,
                                                          uint64_t encoded_tail) {
    if ((1 << kTailSize) != encoded_tail) {
      uint64_t permuted[2], pop[2];
      for (int i : {0, 1}) {
        permuted[i] = f[i].Permute(log_size + 5 + kHeadSize,
                                   (raw << 1) | (encoded_tail << kHeadSize));
        pop[i] = lines[permuted[i] >> (kHeadSize + 5)].Population();
      }
      int lean = pop[0] > pop[1];
      Line& line = lines[permuted[lean] >> (kHeadSize + 5)];
      bool result =
          line.Insert(lean, (permuted[lean] >> kHeadSize) & ((1 << 5) - 1),
                      permuted[lean] & ((1 << kHeadSize) - 1), encoded_tail << 1);
      if (result) ++occupancy;
      return result;
    } else {
      bool result = InsertNeedsNoUpsize(raw, encoded_tail >> 1);
      result = result && InsertNeedsNoUpsize(raw, encoded_tail | (encoded_tail >> 1));
      return result;
    }
  }

  __attribute__((always_inline)) bool Find(uint64_t raw, uint64_t encoded_tail) {
    for (int i : {0, 1}) {
      uint64_t permuted = f[i].Permute(log_size + 5 + kHeadSize,
                                       (raw << 1) | (encoded_tail << kHeadSize));
      if (lines[permuted >> (kHeadSize + 5)].Find(
              i, (permuted >> kHeadSize) & ((1 << 5) - 1),
              permuted & ((1 << kHeadSize) - 1), encoded_tail)) {
        return true;
      }
    }
    return false;
  }

  __attribute__((always_inline)) bool FindHash(uint64_t raw) {
    return Find(raw >> (64 - log_size - 5 - kHeadSize),
                ((raw >> (64 - log_size - 5 - kHeadSize - kTailSize)) &
                 ((1 << kTailSize) - 1) << 1) |
                    1);
  }

  __attribute__((always_inline)) bool InsertHash(uint64_t raw) {
    while (not InsertNeedsNoUpsize(raw >> (64 - log_size - 5 - kHeadSize),
                                   ((raw >> (64 - log_size - 5 - kHeadSize - kTailSize)) &
                                    ((1 << kTailSize) - 1) << 1) |
                                   1)) {
      Upsize();
    }
    return true;
  }

  void Upsize() {
    uint64_t dummy[8] = {};
    int size_up = 1;
  start:
    TaffyVectorQuotientFilter that(log_size + size_up, dummy);
    for (int i : {0, 1}) that.f[i] = f[i];
    for (uint64_t i = 0; i < (1ull << log_size); ++i) {
      Line line = lines[i];
      uint64_t j = 0, m = 0;
      while (j < 28 && m < 32) {
        if ((line.metadata >> (j + m)) & 1) {
          ++m;
        } else {
          Line::Entry e = line.data[j];
          uint64_t permuted = (((i << 5) | m) << kHeadSize) | e.fingerprint;
          uint64_t raw =
              f[e.lean].ReversePermute(log_size + size_up + 5 + kHeadSize, permuted);
          bool ok = that.InsertNeedsNoUpsize(raw, e.tail);
          if (not ok) {
            ++size_up;
            //std::cout << std::dec << log_size << ' ' << size_up << std::endl;
            goto start;
          }
          ++j;
        }
      }
    }
    swap(*this, that);
  }
};

void swap(TaffyVectorQuotientFilter& x, TaffyVectorQuotientFilter& y) {
  using std::swap;
  swap(x.log_size, y.log_size);
  swap(x.lines, y.lines);
  for (int i : {0, 1}) swap(x.f[i], y.f[i]);
}

}  // namespace filter
