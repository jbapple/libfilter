#include "filter/taffy-cuckoo.h"

INLINE void UnionOne(TaffyCuckooFilterBase* here, const TaffyCuckooFilterBase* that) {
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
