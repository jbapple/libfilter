#include "filter/taffy-cuckoo.h"

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
