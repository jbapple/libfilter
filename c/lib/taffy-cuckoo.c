#include "filter/taffy-cuckoo.h"

void TaffyCuckooFilterBaseDestroy(TaffyCuckooFilterBase* t) {
  free(t->sides[0].data);
  free(t->sides[0].stash);
  free(t->sides[1].data);
  free(t->sides[1].stash);
}


// Take an item from slot sl with bucket index i, a filter u that sl is in, a side that
// sl is in, and a filter to move sl to, does so, potentially inserting TWO items in t,
// as described in the paper.
INLINE void UpsizeHelper(TaffyCuckooFilterBase* here, Slot sl, uint64_t i, int s,
                         TaffyCuckooFilterBase* t) {
  if (sl.tail == 0) return;
  Path p;
  p.slot = sl;
  p.bucket = i;
  uint64_t q = FromPathNoTail(p, &here->sides[s].f, here->log_side_size);
  if (sl.tail == 1ul << libfilter_taffy_cuckoo_tail_size) {
    // There are no tail bits left! Insert two values.
    // First, hash to the left side of the larger table.
    p = ToPath(q, &t->sides[0].f, t->log_side_size);
    // Still no tail! :-)
    p.slot.tail = sl.tail;
    InsertTCFB(t, 0, p);
    // change the raw value by just one bit: its last
    q |= (1ul << (64 - here->log_side_size - libfilter_taffy_cuckoo_head_size - 1));
    p = ToPath(q, &t->sides[0].f, t->log_side_size);
    p.slot.tail = sl.tail;
    InsertTCFB(t, 0, p);
  } else {
    // steal a bit from the tail
    q |= ((uint64_t)(sl.tail >> libfilter_taffy_cuckoo_tail_size))
         << (64 - here->log_side_size - libfilter_taffy_cuckoo_head_size - 1);
    Path r = ToPath(q, &t->sides[0].f, t->log_side_size);
    r.slot.tail = (sl.tail << 1);
    InsertTCFB(t, 0, r);
  }
}

void Upsize(TaffyCuckooFilterBase* here) {
  TaffyCuckooFilterBase t =
      TaffyCuckooFilterBaseCreate(1 + here->log_side_size, here->entropy);

  for (int s = 0; s < 2; ++s) {
    for (size_t i = 0; i < here->sides[s].stash_size; ++i) {
      UpsizeHelper(here, here->sides[s].stash[i].slot, here->sides[s].stash[i].bucket, s,
                   &t);
    }
    for (unsigned i = 0; i < (1u << here->log_side_size); ++i) {
      for (int j = 0; j < libfilter_slots; ++j) {
        Slot sl = here->sides[s].data[i].data[j];
        UpsizeHelper(here, sl, i, s, &t);
      }
    }
  }
  //using std::swap;
  TaffyCuckooFilterBaseSwap(here, &t);
  TaffyCuckooFilterBaseDestroy(&t);
}

static void UnionHelp(TaffyCuckooFilterBase* here, const TaffyCuckooFilterBase* that,
                      int side, Path p) {
  uint64_t hashed = FromPathNoTail(p, &that->sides[side].f, that->log_side_size);
  // hashed is high that->log_side_size + libfilter_taffy_cuckoo_head_size, in high bits of 64-bit word
  int tail_size = libfilter_taffy_cuckoo_tail_size - __builtin_ctz(p.slot.tail);
  if (that->log_side_size == here->log_side_size) {
    Path q = ToPath(hashed, &here->sides[0].f, here->log_side_size);
    q.slot.tail = p.slot.tail;
    InsertTCFB(here, 0, q);
    q.slot.tail = 0;  // dummy line just to break on
  } else if (that->log_side_size + tail_size >= here->log_side_size) {
    uint64_t orin3 = (((uint64_t)(p.slot.tail & (p.slot.tail - 1)))
                      << (64 - that->log_side_size - libfilter_taffy_cuckoo_head_size - libfilter_taffy_cuckoo_tail_size - 1));
    assert((hashed & orin3) == 0);
    hashed |= orin3;
    Path q = ToPath(hashed, &here->sides[0].f, here->log_side_size);
    q.slot.tail = (p.slot.tail << (here->log_side_size - that->log_side_size));
    InsertTCFB(here, 0, q);
  } else {
    // p.tail & (p.tail - 1) removes the final 1 marker. The resulting length is
    // 0, 1, 2, 3, 4, or 5. It is also tail_size, but is packed in high bits of a
    // section with size libfilter_taffy_cuckoo_tail_size + 1.
    uint64_t orin2 =
      (((uint64_t)(p.slot.tail & (p.slot.tail - 1)))
         << (64 - that->log_side_size - libfilter_taffy_cuckoo_head_size - libfilter_taffy_cuckoo_tail_size - 1));
    assert(0 == (orin2 & hashed));
    hashed |= orin2;
    // The total size is now that->log_side_size + libfilter_taffy_cuckoo_head_size + tail_size
    //
    // To fill up the required log_size_size + libfilter_taffy_cuckoo_head_size, we need values of width up to
    // log_size_size + libfilter_taffy_cuckoo_head_size - (that->log_side_size + libfilter_taffy_cuckoo_head_size +
    // tail_size)
    for (uint64_t i = 0;
         i < (1u << (here->log_side_size - that->log_side_size - tail_size)); ++i) {
      // To append these, need to shift up to that->log_side_size + libfilter_taffy_cuckoo_head_size +
      // tail_size
      uint64_t orin = (i << (64 - here->log_side_size - libfilter_taffy_cuckoo_head_size));
      assert(0 == (orin & hashed));
      uint64_t tmphashed = (hashed | orin);
      Path q = ToPath(tmphashed, &here->sides[0].f, here->log_side_size);
      q.slot.tail = (1u << libfilter_taffy_cuckoo_tail_size);
      InsertTCFB(here, 0, q);
    }
  }
}

static void UnionOne(TaffyCuckooFilterBase* here, const TaffyCuckooFilterBase* that) {
  assert(that->log_side_size <= here->log_side_size);
  Path p;
  for (int side = 0; side < 2; ++side) {
    for (size_t i = 0; i < that->sides[side].stash_size; ++i) {
      UnionHelp(here, that, side, that->sides[side].stash[i]);
    }
    for (uint64_t bucket = 0; bucket < (1ul << that->log_side_size); ++bucket) {
      p.bucket = bucket;
      for (int slot = 0; slot < libfilter_slots; ++slot) {
        if (that->sides[side].data[bucket].data[slot].tail == 0) continue;
        p.slot.fingerprint = that->sides[side].data[bucket].data[slot].fingerprint;
        p.slot.tail = that->sides[side].data[bucket].data[slot].tail;
        UnionHelp(here, that, side, p);
        continue;
      }
    }
  }
}

TaffyCuckooFilterBase UnionTwo(const TaffyCuckooFilterBase* x, const TaffyCuckooFilterBase* y) {
  if (x->occupied > y->occupied) {
    TaffyCuckooFilterBase result = TaffyCuckooFilterBaseClone(x);
    UnionOne(&result, y);
    return result;
  }
  TaffyCuckooFilterBase result = TaffyCuckooFilterBaseClone(y);
  UnionOne(&result, x);
  return result;
}
