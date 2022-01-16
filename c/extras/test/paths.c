#include "filter/util.h"
#include "filter/paths.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#if defined(NDEBUG)
#error NDEBUG
#endif

void from_to() {
  printf("%s\n", __FUNCTION__);
  uint64_t entropy[4] = {1, 0, 1, 0};
  libfilter_feistel f = libfilter_feistel_create(entropy);
  uint64_t xbase = 0x123456789abcdef0;
  int low_level = 16;
  unsigned many  = 0;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      for (int is_short = 0; is_short < 2; ++is_short) {
        uint64_t x = xbase << i;
        libfilter_minimal_taffy_cuckoo_path p =
            libfilter_minimal_taffy_cuckoo_to_path(x, &f, cursor, low_level, is_short);
        if (p.slot.tail == 0) continue;
        ++many;
        uint64_t y = libfilter_minimal_taffy_cuckoo_from_path_no_tail(
            p, &f, low_level + (p.level < cursor),
            libfilter_minimal_taffy_cuckoo_head_size - (1 - p.slot.long_fp));
        int shift = 64 - (libfilter_minimal_taffy_cuckoo_log_levels + low_level +
                          (p.level < cursor) + libfilter_minimal_taffy_cuckoo_head_size -
                          (1 - p.slot.long_fp));
        assert(x >> shift == y >> shift);
      }
    }
  }
  assert(many >= 64 * 32u);
}

void to_from_to() {
  printf("%s\n", __FUNCTION__);
  uint64_t entropy[4] = {1, 0, 1, 0};
  libfilter_feistel f = libfilter_feistel_create(entropy);
  uint64_t xbase = 0x123456789abcdef0;
  int low_level = 16;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      for (int is_short = 0; is_short < 2; ++is_short) {
        uint64_t x = xbase << i;
        libfilter_minimal_taffy_cuckoo_path p =
            libfilter_minimal_taffy_cuckoo_to_path(x, &f, cursor, low_level, is_short);
        if (p.slot.tail == 0) continue;
        uint64_t y = libfilter_minimal_taffy_cuckoo_from_path_no_tail(
            p, &f, low_level + (p.level < cursor),
            libfilter_minimal_taffy_cuckoo_head_size - (1 - p.slot.long_fp));
        libfilter_minimal_taffy_cuckoo_path q =
            libfilter_minimal_taffy_cuckoo_to_path(y, &f, cursor, low_level, is_short);
        assert(p.level == q.level);
        assert(p.bucket == q.bucket);
        assert(p.slot.fingerprint == q.slot.fingerprint);
        assert(p.slot.long_fp == q.slot.long_fp);
        // EXPECT_EQ(0u, q.tail);
      }
    }
  }
}

void re_path_half_identity() {
  printf("%s\n", __FUNCTION__);
  uint64_t entropy[4] = {1, 0, 1, 0};
  libfilter_feistel identity  = libfilter_feistel_create(entropy);
  uint64_t entropy2[4] = {0x37156873ab534ce7, 0x5c669c3116114489, 0xfa52f24f2bc644d6,
                          0xcba217328d2f4950};
  libfilter_feistel f = libfilter_feistel_create(entropy2);
  uint64_t xbase = 0x123456789abcdef0;
  int low_level = 16;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      bool is_short = false;
      uint64_t x = xbase << i;
      libfilter_minimal_taffy_cuckoo_path p = libfilter_minimal_taffy_cuckoo_to_path(
          x, &identity, cursor, low_level, is_short);
      if (p.slot.tail == 0) continue;
      libfilter_minimal_taffy_cuckoo_path r;
      libfilter_minimal_taffy_cuckoo_path q = libfilter_minimal_taffy_cuckoo_re_path(
          p, &identity, &identity, &f, &f, low_level, low_level, cursor, cursor, &r);
      assert(r.slot.tail == 0u);
      r = libfilter_minimal_taffy_cuckoo_to_path(x, &f, cursor, low_level, is_short);
      assert(r.slot.tail != 0u);
      assert(q.level == r.level);
      assert(q.bucket == r.bucket);
      assert(q.slot.fingerprint == r.slot.fingerprint);
      assert(q.slot.long_fp == r.slot.long_fp);
      assert(q.slot.tail == r.slot.tail);
    }
  }
}

void re_path_short_identity() {
  printf("%s\n", __FUNCTION__);
  uint64_t entropy[4] = {1, 0, 1, 0};
  libfilter_feistel identity = libfilter_feistel_create(entropy);
  uint64_t entropy2[4] = {0x37156873ab534ce7, 0x5c669c3116114489, 0xfa52f24f2bc644d6,
                          0xcba217328d2f4950};
  libfilter_feistel f = libfilter_feistel_create(entropy2);
  uint64_t xbase = 0x123456789abcdef0;
  int low_level = 16;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      bool is_short = true;
      uint64_t x = xbase << i;
      libfilter_minimal_taffy_cuckoo_path p = libfilter_minimal_taffy_cuckoo_to_path(
          x, &identity, cursor, low_level, is_short);
      libfilter_minimal_taffy_cuckoo_path q =
          libfilter_minimal_taffy_cuckoo_to_path(x, &f, cursor, low_level, is_short);
      if (p.slot.tail == 0 || q.slot.tail == 0) {
        continue;
      }
      libfilter_minimal_taffy_cuckoo_path r;
      libfilter_minimal_taffy_cuckoo_path s = libfilter_minimal_taffy_cuckoo_re_path(
          p, &identity, &identity, &f, &f, low_level, low_level, cursor, cursor, &r);
      assert(r.slot.tail == 0u);
      assert(q.level == s.level);
      assert(q.bucket == s.bucket);
      assert(q.slot.fingerprint == s.slot.fingerprint);
      assert(q.slot.long_fp == s.slot.long_fp);
      assert(q.slot.tail == s.slot.tail);
    }
  }
}


void re_path_short_long_identity() {
  printf("%s\n", __FUNCTION__);
  uint64_t entropy[4] = {1, 0, 1, 0};
  libfilter_feistel identity = libfilter_feistel_create(entropy);
  uint64_t entropy2[4] = {0x37156873ab534ce7, 0x5c669c3116114489, 0xfa52f24f2bc644d6,
                          0xcba217328d2f4950};
  libfilter_feistel f = libfilter_feistel_create(entropy2);
  uint64_t xbase = 0x123456789abcdef0;
  int low_level = 16;
  unsigned count = 0;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      uint64_t x = xbase << i;
      libfilter_minimal_taffy_cuckoo_path p =
          libfilter_minimal_taffy_cuckoo_to_path(x, &identity, cursor, low_level, true);
      if (p.slot.tail == 0) continue;
      libfilter_minimal_taffy_cuckoo_path q =
          libfilter_minimal_taffy_cuckoo_to_path(x, &f, cursor, low_level, true);
      if (q.slot.tail != 0) continue;
      count++;
      q = libfilter_minimal_taffy_cuckoo_to_path(x, &f, cursor, low_level, false);
      libfilter_minimal_taffy_cuckoo_path r;
      libfilter_minimal_taffy_cuckoo_path s = libfilter_minimal_taffy_cuckoo_re_path(
          p, &identity, &identity, &f, &f, low_level, low_level, cursor, cursor, &r);
      assert(r.slot.tail == 0u);
      assert(q.level == s.level);
      assert(q.bucket == s.bucket);
      assert(q.slot.fingerprint == s.slot.fingerprint);
      assert(q.slot.long_fp == s.slot.long_fp);
      assert(libfilter_taffy_is_prefix_of(s.slot.tail, q.slot.tail));
    }
  }
  assert(count >= 100u);
}


// Test RePath when there are two outputs
void re_path_double() {
  printf("%s\n", __FUNCTION__);
  uint64_t entropy[4] = {1, 0, 1, 0};
  libfilter_feistel identity = libfilter_feistel_create(entropy);
  uint64_t entropy2[4] = {0x37156873ab534ce7, 0x5c669c3116114489, 0xfa52f24f2bc644d6,
                          0xcba217328d2f4950};
  libfilter_feistel f = libfilter_feistel_create(entropy2);
  uint64_t xbase = 0x123456789abcdef0;
  int low_level = 16;
  unsigned count = 0;
  for (int i = 0; i < 64; ++i) {
    for (int cursor = 0; cursor < 32; ++cursor) {
      uint64_t x = xbase << i;
      libfilter_minimal_taffy_cuckoo_path p =
          libfilter_minimal_taffy_cuckoo_to_path(x, &identity, cursor, low_level, true);
      if (p.slot.tail == 0) continue;
      libfilter_minimal_taffy_cuckoo_path q =
          libfilter_minimal_taffy_cuckoo_to_path(x, &f, cursor, low_level, true);
      if (q.slot.tail != 0) continue;
      count++;
      q = libfilter_minimal_taffy_cuckoo_to_path(x, &f, cursor, low_level, false);
      libfilter_minimal_taffy_cuckoo_path r;
      p.slot.tail = 1u << libfilter_minimal_taffy_cuckoo_tail_size;
      libfilter_minimal_taffy_cuckoo_path s = libfilter_minimal_taffy_cuckoo_re_path(
          p, &identity, &identity, &f, &f, low_level, low_level, cursor, cursor, &r);
      assert(s.slot.tail == (1u << libfilter_minimal_taffy_cuckoo_tail_size));
      assert(r.slot.tail ==( 1u << libfilter_minimal_taffy_cuckoo_tail_size));
      assert((q.level == s.level && q.bucket == s.bucket &&
                   q.slot.fingerprint == s.slot.fingerprint &&
                   q.slot.long_fp == s.slot.long_fp) ||
                  (q.level == r.level && q.bucket == r.bucket &&
                   q.slot.fingerprint == r.slot.fingerprint &&
                   q.slot.long_fp == r.slot.long_fp));
    }
  }
  assert(count >= 100u);
}

int main() {
  from_to();
  to_from_to();
  re_path_half_identity();
  re_path_short_identity();
  re_path_short_long_identity();
  re_path_double();
}
