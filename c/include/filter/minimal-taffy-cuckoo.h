#pragma once

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#include "filter/paths.h"
#include "filter/taffy-cuckoo.h"
#include "filter/util.h"

typedef struct {
  libfilter_minimal_taffy_cuckoo_slot data[libfilter_slots];
} libfilter_minimal_taffy_cuckoo_bucket;

// static_assert(sizeof(libfilter_minimal_taffy_cuckoo_bucket) ==
//                   sizeof(libfilter_minimal_taffy_cuckoo_slot) * libfilter_slots,
//               "sizeof(Bucket)");

typedef struct {
  libfilter_minimal_taffy_cuckoo_bucket* data;
}  libfilter_minimal_taffy_cuckoo_level;

// Returns an empty path (tail = 0) if insert added a new element. Returns p if insert
// succeded without anning anything new. Returns something else if that something else
// was displaced by the insert. That item must be inserted then
INLINE libfilter_minimal_taffy_cuckoo_path libfilter_minimal_taffy_cuckoo_level_insert(
    libfilter_minimal_taffy_cuckoo_level* here, libfilter_minimal_taffy_cuckoo_path p,
    libfilter_pcg_random* rng) {
  assert(p.slot.tail != 0);
  libfilter_minimal_taffy_cuckoo_bucket * b = &here->data[p.bucket];
  for (int i = 0; i < libfilter_slots; ++i) {
    if (b->data[i].tail == 0) {
      // empty slot
      b->data[i] = p.slot;
      p.slot.tail = 0;
      return p;
    }
    if (b->data[i].long_fp == p.slot.long_fp &&
        b->data[i].fingerprint == p.slot.fingerprint) {
      // already present in the table
      if (libfilter_taffy_is_prefix_of(b->data[i].tail, p.slot.tail)) {
        return p;
      }
      // This has a negligible effect on fpp
      // auto c = Combinable(b[i].tail, p.tail);
      // if (c > 0) {
      //   b[i].tail = c;
      //   return p;
      // }
    }
  }
  // Kick something random and return it
  int i = libfilter_pcg_random_get(rng);
  libfilter_minimal_taffy_cuckoo_path result = p;
  result.slot = b->data[i];
  b->data[i] = p.slot;
  return result;
}

INLINE bool libfilter_minimal_taffy_cuckoo_level_find(
    const libfilter_minimal_taffy_cuckoo_level* here,
    libfilter_minimal_taffy_cuckoo_path p) {
  assert(p.slot.tail != 0);
  libfilter_minimal_taffy_cuckoo_bucket* b = &here->data[p.bucket];
  for (int i = 0; i < libfilter_slots; ++i) {
    if (b->data[i].tail == 0) continue;
    if (b->data[i].long_fp == p.slot.long_fp &&
        b->data[i].fingerprint == p.slot.fingerprint &&
        libfilter_taffy_is_prefix_of(b->data[i].tail, p.slot.tail)) {
      return true;
    }
  }
  return false;
}

// A cuckoo hash table is made up of sides (in some incarnations), rather than jut two
// buckets from one array. Each side has a stash that holds any paths that couldn't fit.
// This is useful for random-walk cuckoo hashing, in which the leftover path needs a place
// to be stored so it doesn't invalidate old inserts.
typedef struct {
  // TODO: can use identity for one side?
  libfilter_feistel hi, lo;
  libfilter_minimal_taffy_cuckoo_level levels[libfilter_minimal_taffy_cuckoo_levels];
  libfilter_minimal_taffy_cuckoo_path * stashes;
  size_t stashes_size, stashes_capacity;
} libfilter_minimal_taffy_cuckoo_side;

INLINE bool libfilter_minimal_taffy_cuckoo_side_find(
    const libfilter_minimal_taffy_cuckoo_side* side,
    libfilter_minimal_taffy_cuckoo_path p) {
  for(size_t i = 0; i < side->stashes_size; ++i) {
    if (side->stashes[i].slot.tail != 0 &&
        side->stashes[i].slot.long_fp == p.slot.long_fp &&
        side->stashes[i].slot.fingerprint == p.slot.fingerprint &&
        libfilter_taffy_is_prefix_of(side->stashes[i].slot.tail, p.slot.tail) &&
        side->stashes[i].level == p.level && side->stashes[i].bucket == p.bucket) {
      return true;
    }
  }
  return libfilter_minimal_taffy_cuckoo_level_find(&side->levels[p.level], p);
}

// Returns an empty path (tail = 0) if insert added a new element. Returns p if insert
// succeded without adding anything new. Returns something else if that something else
// was displaced by the insert. That item must be inserted then.
INLINE libfilter_minimal_taffy_cuckoo_path libfilter_minimal_taffy_cuckoo_side_insert(
    libfilter_minimal_taffy_cuckoo_side* s, libfilter_minimal_taffy_cuckoo_path p,
    libfilter_pcg_random* rng) {
  libfilter_minimal_taffy_cuckoo_path result =
      libfilter_minimal_taffy_cuckoo_level_insert(&s->levels[p.level], p, rng);
  assert(libfilter_minimal_taffy_cuckoo_side_find(s, p));
  return result;
}

typedef struct {
  libfilter_minimal_taffy_cuckoo_side sides[2];
  uint64_t cursor;
  uint64_t log_side_size;
  libfilter_pcg_random rng;
  uint64_t occupied;
} libfilter_minimal_taffy_cuckoo;

void libfilter_minimal_taffy_cuckoo_null_out(libfilter_minimal_taffy_cuckoo*);

void libfilter_minimal_taffy_cuckoo_destruct(libfilter_minimal_taffy_cuckoo*);

libfilter_minimal_taffy_cuckoo libfilter_minimal_taffy_cuckoo_create(
    int log_side_size, const uint64_t* entropy);

INLINE uint64_t libfilter_minimal_taffy_cuckoo_capacity(
    const libfilter_minimal_taffy_cuckoo* here) {
  return 2 + 2 * libfilter_slots *
                 ((1ul << here->log_side_size) * libfilter_minimal_taffy_cuckoo_levels +
                  (1ul << here->log_side_size) * here->cursor);
}

uint64_t libfilter_minimal_taffy_cuckoo_size_in_bytes(
    const libfilter_minimal_taffy_cuckoo*);

libfilter_minimal_taffy_cuckoo libfilter_minimal_taffy_cuckoo_create_with_bytes(
    uint64_t bytes);

INLINE bool libfilter_minimal_taffy_cuckoo_find_hash(
    const libfilter_minimal_taffy_cuckoo* here, uint64_t k) {
  for (int i = 0; i < 2; ++i) {
    libfilter_minimal_taffy_cuckoo_path p = libfilter_minimal_taffy_cuckoo_to_path(
        k, &here->sides[i].lo, here->cursor, here->log_side_size, true);
    if (p.slot.tail != 0 &&
        libfilter_minimal_taffy_cuckoo_side_find(&here->sides[i], p)) {
      return true;
    }
    p = libfilter_minimal_taffy_cuckoo_to_path(k, &here->sides[i].hi, here->cursor,
                                               here->log_side_size, false);
    if (p.slot.tail != 0 &&
        libfilter_minimal_taffy_cuckoo_side_find(&here->sides[i], p)) {
      return true;
    }
  }
  return false;
}

void libfilter_minimal_taffy_cuckoo_upsize(libfilter_minimal_taffy_cuckoo* here);
void libfilter_minimal_taffy_cuckoo_insert_detail(libfilter_minimal_taffy_cuckoo* here,
                                                  int side,
                                                  libfilter_minimal_taffy_cuckoo_path p,
                                                  int ttl);

INLINE bool libfilter_minimal_taffy_cuckoo_add_hash(
    libfilter_minimal_taffy_cuckoo* here, uint64_t k) {
  while (here->occupied > 0.9 * libfilter_minimal_taffy_cuckoo_capacity(here) ||
         here->occupied + 4 >= libfilter_minimal_taffy_cuckoo_capacity(here) ||
         here->sides[0].stashes_size + here->sides[1].stashes_size > 8) {
    libfilter_minimal_taffy_cuckoo_upsize(here);
  }
  // TODO: only need one path here. Which one to pick?
  libfilter_minimal_taffy_cuckoo_path p = libfilter_minimal_taffy_cuckoo_to_path(
      k, &here->sides[0].hi, here->cursor, here->log_side_size, false);
  libfilter_minimal_taffy_cuckoo_insert_detail(here, 0, p, 128);
  return true;
}

inline void libfilter_minimal_taffy_cuckoo_insert_detail(
    libfilter_minimal_taffy_cuckoo* here, int side, libfilter_minimal_taffy_cuckoo_path p,
    int ttl) {
  assert(p.slot.tail != 0);
  while (true) {
    for (int j = 0; j < 2; ++j) {
      int tmp[2] = {side, 1 - side};
      int i = tmp[j];
      --ttl;
      if (ttl < 0) {
        if (here->sides[i].stashes_size == here->sides[i].stashes_capacity) {
          here->sides[i].stashes = (libfilter_minimal_taffy_cuckoo_path*)realloc(
              here->sides[i].stashes, 2 * here->sides[i].stashes_capacity *
                                          sizeof(libfilter_minimal_taffy_cuckoo_path));
          here->sides[i].stashes_capacity *= 2;
        }
        here->sides[i].stashes[here->sides[i].stashes_size++] = p;
        ++here->occupied;
        return;
      }
      libfilter_minimal_taffy_cuckoo_path q = p;
      libfilter_minimal_taffy_cuckoo_path r =
          libfilter_minimal_taffy_cuckoo_side_insert(&here->sides[i], p, &here->rng);
      if (r.slot.tail == 0) {
        // Found an empty slot
        ++here->occupied;
        return;
      }
      if (libfilter_minimal_taffy_cuckoo_path_equal(r, q)) {
        // Combined with or already present in a slot. Success, but no increase in
        // filter size
        return;
      }
      libfilter_minimal_taffy_cuckoo_path extra;
      libfilter_minimal_taffy_cuckoo_path next = libfilter_minimal_taffy_cuckoo_re_path(
          r, &here->sides[i].lo, &here->sides[i].hi, &here->sides[1 - i].lo, &here->sides[1 - i].hi,
          here->log_side_size, here->log_side_size, here->cursor, here->cursor, &extra);
      if (extra.slot.tail != 0) {
        libfilter_minimal_taffy_cuckoo_insert_detail(here, 1 - i, extra, ttl);
      }
      // TODO: what if insert returns stashed? Do we need multiple states? Maybe green ,
      // yellow red? Or maybe break at the beginning of this logic if repath returns two
      // things or retry if there aren't two stashes open at this time.
      p = next;
      assert(p.slot.tail != 0);
    }
  }
}
