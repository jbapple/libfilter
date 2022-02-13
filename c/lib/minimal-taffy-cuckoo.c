#include "filter/minimal-taffy-cuckoo.h"

void libfilter_minimal_taffy_cuckoo_level_null_out(
    libfilter_minimal_taffy_cuckoo_level* here) {
  here->data = NULL;
}

INLINE libfilter_minimal_taffy_cuckoo_level
libfilter_minimal_taffy_cuckoo_level_create(int log_level_size) {
  libfilter_minimal_taffy_cuckoo_level result;
  result.data = (libfilter_minimal_taffy_cuckoo_bucket*)calloc(
      1ull << log_level_size, sizeof(libfilter_minimal_taffy_cuckoo_bucket));
  return result;
}

INLINE void libfilter_minimal_taffy_cuckoo_level_destroy(
    libfilter_minimal_taffy_cuckoo_level* l) {
  free(l->data);
}

void libfilter_minimal_taffy_cuckoo_side_null_out(libfilter_minimal_taffy_cuckoo_side * here) {
  for(unsigned i = 0; i < libfilter_minimal_taffy_cuckoo_levels; ++i) {
    libfilter_minimal_taffy_cuckoo_level_null_out(&here->levels[i]);
  }
  here->stashes = NULL;
}

libfilter_minimal_taffy_cuckoo_side libfilter_minimal_taffy_cuckoo_side_create(
    int log_level_size, const uint64_t* keys) {
  libfilter_minimal_taffy_cuckoo_side result;
  result.hi = libfilter_feistel_create(&keys[0]);
  result.lo = libfilter_feistel_create(&keys[6]);
  for (uint64_t i = 0; i < libfilter_minimal_taffy_cuckoo_levels; ++i) {
    result.levels[i] = libfilter_minimal_taffy_cuckoo_level_create(log_level_size);
  }
  result.stashes = (libfilter_minimal_taffy_cuckoo_path*)malloc(
      sizeof(libfilter_minimal_taffy_cuckoo_path) * 4);
  result.stashes_capacity = 4;
  result.stashes_size = 0;
  return result;
}

void libfilter_minimal_taffy_cuckoo_side_destroy(
    libfilter_minimal_taffy_cuckoo_side* side) {
  for (unsigned i = 0; i < libfilter_minimal_taffy_cuckoo_levels; ++i) {
    libfilter_minimal_taffy_cuckoo_level_destroy(&side->levels[i]);
  }
  free(side->stashes);
}

void libfilter_minimal_taffy_cuckoo_null_out(libfilter_minimal_taffy_cuckoo * here) {
  for(int i = 0; i < 2; ++i) {
    libfilter_minimal_taffy_cuckoo_side_null_out(&here->sides[i]);
  }
}

void libfilter_minimal_taffy_cuckoo_destruct(libfilter_minimal_taffy_cuckoo* here) {
  libfilter_minimal_taffy_cuckoo_side_destroy(&here->sides[0]);
  libfilter_minimal_taffy_cuckoo_side_destroy(&here->sides[1]);
}

libfilter_minimal_taffy_cuckoo libfilter_minimal_taffy_cuckoo_create(
    int log_side_size, const uint64_t* entropy) {
  libfilter_minimal_taffy_cuckoo result;
  result.sides[0] = libfilter_minimal_taffy_cuckoo_side_create(log_side_size, entropy);
  result.sides[1] =
      libfilter_minimal_taffy_cuckoo_side_create(log_side_size, entropy + 12);
  result.cursor = 0;
  result.log_side_size = log_side_size;
  result.rng = libfilter_pcg_random_create(libfilter_log_slots);
  result.occupied = 0;
  return result;
}

uint64_t libfilter_minimal_taffy_cuckoo_size_in_bytes(
    const libfilter_minimal_taffy_cuckoo* here) {
  return sizeof(libfilter_minimal_taffy_cuckoo_slot) *
             libfilter_minimal_taffy_cuckoo_capacity(here) +
         2 * (sizeof(libfilter_minimal_taffy_cuckoo_path) -
              sizeof(libfilter_minimal_taffy_cuckoo_slot));
}

INLINE libfilter_minimal_taffy_cuckoo
libfilter_minimal_taffy_cuckoo_create_with_bytes(uint64_t bytes) {
  (void)bytes;
  static const uint64_t kEntropy[24] = {
      0x2ba7538ee1234073, 0xfcc3777539b147d6, 0x6086c563576347e7, 0x52eff34ee1764465,
      0x8639cbf57f264867, 0x5a31ee34f0224ccb, 0x07a1cb8140744ee6, 0xf2296cf6a6524e9f,
      0x28a31cec9f6d4484, 0x688f3fe9de7245f6, 0x1dc17831966b41a2, 0xf227166e425e4b0c,
      0x4a2a62bafc694440, 0x2e6bbea775e3429d, 0x5687dd060ba64169, 0xc5d95e8a38a44789,
      0xd30480ab74084edc, 0xd72483670ec14df3, 0x0414954940374787, 0x8cd86adfda93493f,
      0x50d61c3272a24ccb, 0x40cb1e4f0da34cc3, 0xb88f09c3af35472e, 0x8de6d01bb8a849a5};
  // TODO: start with a size other than 0
  return libfilter_minimal_taffy_cuckoo_create(
      0,
      // std::max(1.0, log(1.0 * bytes / 2 / kBuckets /
      //                   sizeof(Slot) /
      //                   kBuckets /
      //                   kLevels) /
      //                   log(2)),
      kEntropy);
}

// Double the size of one level of the filter
INLINE void libfilter_minimal_taffy_cuckoo_upsize(libfilter_minimal_taffy_cuckoo* here) {
  libfilter_minimal_taffy_cuckoo_bucket* last_data[2] = {here->sides[0].levels[here->cursor].data,
                                                         here->sides[1].levels[here->cursor].data};
  {
    libfilter_minimal_taffy_cuckoo_bucket* next[2];
    for (int i = 0; i < 2; ++i) {
      next[i] = (libfilter_minimal_taffy_cuckoo_bucket*)calloc(
          2ull << here->log_side_size, sizeof(libfilter_minimal_taffy_cuckoo_bucket));
    }

    here->sides[0].levels[here->cursor].data = next[0];
    here->sides[1].levels[here->cursor].data = next[1];
  }
  here->cursor = here->cursor + 1;
  libfilter_minimal_taffy_cuckoo_path p;
  p.level = here->cursor - 1;

  libfilter_minimal_taffy_cuckoo_path* stashes[2];
  size_t stash_capacities[2] = {4, 4}, stash_sizes[2] = {0, 0};
  for (int i = 0; i < 2; ++i) {
    stashes[i] = (libfilter_minimal_taffy_cuckoo_path*)malloc(
        4 * sizeof(libfilter_minimal_taffy_cuckoo_path));
    libfilter_minimal_taffy_cuckoo_path* tmp = stashes[i];
    stashes[i] = here->sides[i].stashes;
    here->sides[i].stashes = tmp;

    size_t stmp = stash_sizes[i];
    stash_sizes[i] = here->sides[i].stashes_size;
    here->sides[i].stashes_size = stmp;

    stmp = stash_capacities[i];
    stash_capacities[i] = here->sides[i].stashes_capacity;
    here->sides[i].stashes_capacity = stash_capacities[i];

    here->occupied = here->occupied - stash_sizes[i];
    here->sides[i].stashes = (libfilter_minimal_taffy_cuckoo_path*)realloc(
        here->sides[i].stashes, 4 * sizeof(libfilter_minimal_taffy_cuckoo_path));
    here->sides[i].stashes_capacity = 4;
    here->sides[i].stashes_size = 0;
  }

  for (int s = 0; s < 2; ++s) {
    for (unsigned i = 0; i < stash_sizes[s]; ++i) {
      libfilter_minimal_taffy_cuckoo_path q, r;
      r = libfilter_minimal_taffy_cuckoo_re_path_upsize(
          stashes[s][i], &here->sides[s].lo, &here->sides[s].hi, here->log_side_size,
          here->cursor - 1, &q);
      int ttl = 128;
      assert(r.slot.tail != 0);
      if (q.slot.tail != 0) {
        libfilter_minimal_taffy_cuckoo_insert_detail(here, s, q, ttl);
      }
      libfilter_minimal_taffy_cuckoo_insert_detail(here, s, r, ttl);
    }
  }
  for (int s = 0; s < 2; ++s) {
    for (unsigned i = 0; i < (1u << here->log_side_size); ++i) {
      p.bucket = i;
      for (int j = 0; j < libfilter_slots; ++j) {
        if (last_data[s][i].data[j].tail == 0) continue;
        --here->occupied;
        p.slot = last_data[s][i].data[j];
        assert(p.slot.tail != 0);
        libfilter_minimal_taffy_cuckoo_path q, r;
        r = libfilter_minimal_taffy_cuckoo_re_path_upsize(p, &here->sides[s].lo, &here->sides[s].hi,
                                                          here->log_side_size, here->cursor - 1, &q);
        int ttl = 128;
        assert(r.slot.tail != 0);
        if (q.slot.tail != 0) {
          libfilter_minimal_taffy_cuckoo_insert_detail(here, s, q, ttl);
        }
        libfilter_minimal_taffy_cuckoo_insert_detail(here, s, r, ttl);
      }
    }
  }

  if (here->cursor == libfilter_minimal_taffy_cuckoo_levels) {
    here->cursor = 0;
    ++here->log_side_size;
    for (int i = 0; i < 2; ++i) {
      libfilter_feistel f = here->sides[i].lo;
      here->sides[i].lo = here->sides[i].hi;
      here->sides[i].hi = f;
    }
  }
  free(stashes[0]);
  free(stashes[1]);
  free(last_data[0]);
  free(last_data[1]);
}
