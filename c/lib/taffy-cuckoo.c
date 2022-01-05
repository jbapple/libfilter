#include "filter/taffy-cuckoo.h"

void TaffyCuckooFilterBaseSwap(TaffyCuckooFilterBase* x, TaffyCuckooFilterBase* y) {
  // SideSwap(&x->sides[0], &y->sides[0]);
  // SideSwap(&x->sides[1], &y->sides[1]);

  // int tmp_int = x->log_side_size;
  // x->log_side_size = y->log_side_size;
  // y->log_side_size = tmp_int;

  TaffyCuckooFilterBase tmp = *x;
  *x = *y;
  *y = tmp;
}

TaffyCuckooFilterBase TaffyCuckooFilterBaseCreate(int log_side_size,
                                                  const uint64_t* entropy) {
  TaffyCuckooFilterBase here;
  here.sides[0] = SideCreate(log_side_size, entropy);
  here.sides[1] = SideCreate(log_side_size, entropy + 4);
  here.log_side_size = log_side_size;
  here.rng = detail_PcgRandomCreate(libfilter_log_slots);
  here.entropy = entropy;
  here.occupied = 0;
  return here;
}

TaffyCuckooFilterBase TaffyCuckooFilterBaseClone(const TaffyCuckooFilterBase* that) {
  TaffyCuckooFilterBase here;
  here.sides[0] = SideCreate(that->log_side_size, that->entropy);
  here.sides[1] = SideCreate(that->log_side_size, that->entropy + 4);
  here.log_side_size = that->log_side_size;
  here.rng = that->rng;
  here.entropy = that->entropy;
  here.occupied = that->occupied;
  for (int i = 0; i < 2; ++i) {
    free( here.sides[i].stash);
    here.sides[i].stash = (Path *)calloc(that->sides[i].stash_capacity, sizeof(Path));
    here.sides[i].stash_capacity = that->sides[i].stash_capacity;
    here.sides[i].stash_size = that->sides[i].stash_size;
    memcpy(&here.sides[i].stash[0], &that->sides[i].stash[0],
           that->sides[i].stash_size * sizeof(Path));
    memcpy(&here.sides[i].data[0], &that->sides[i].data[0],
           sizeof(Bucket) << that->log_side_size);
  }
  return here;
}

TaffyCuckooFilterBase BaseCreateWithBytes(uint64_t bytes) {
  static const uint64_t kEntropy[8] = {
      0x2ba7538ee1234073, 0xfcc3777539b147d6, 0x6086c563576347e7, 0x52eff34ee1764465,
      0x8639cbf57f264867, 0x5a31ee34f0224ccb, 0x07a1cb8140744ee6, 0xf2296cf6a6524e9f};
  double f = log(1.0 * bytes / 2 / libfilter_slots / sizeof(Slot)) / log(2);
  f = (f > 1.0) ? f : 1.0;
  return TaffyCuckooFilterBaseCreate(f, kEntropy);
}

FrozenTaffyCuckooBase BaseFreeze(const TaffyCuckooFilterBase* here) {
  FrozenTaffyCuckooBase result =
      FrozenTaffyCuckooBaseCreate(here->entropy, here->log_side_size);
  for (int i = 0; i < 2; ++i) {
    for (size_t j = 0; j < here->sides[i].stash_size; ++j) {
      uint64_t topush =
          FromPathNoTail(here->sides[i].stash[j], &here->sides[i].f, here->log_side_size);
      if (result.stash_size_[i] == result.stash_capacity_[i]) {
        result.stash_capacity_[i] *= 2;
        uint64_t* new_stash = (uint64_t*)calloc(result.stash_capacity_[i], sizeof(uint64_t));
        memcpy(new_stash, result.stash_[i], result.stash_size_[i]);
        free( result.stash_[i]);
        result.stash_[i] = new_stash;
      }
      result.stash_[i][result.stash_size_[i]++] = topush;
    }
    for (size_t j = 0; j < (1ul << here->log_side_size); ++j) {
      FrozenTaffyCuckooBaseBucket* out = &result.data_[i][j];
      const Bucket* in = &here->sides[i].data[j];
      out->zero = in->data[0].fingerprint;
      out->one = in->data[1].fingerprint;
      out->two = in->data[2].fingerprint;
      out->three = in->data[3].fingerprint;
    }
  }
  return result;
}

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
