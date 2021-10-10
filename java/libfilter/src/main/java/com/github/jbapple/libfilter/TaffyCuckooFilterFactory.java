package com.github.jbapple.libfilter;

public class TaffyCuckooFilterFactory
    implements FilterWithBytesFactory<TaffyCuckooFilter> {
  @Override
  public TaffyCuckooFilter CreateWithBytes(int n) {
    return new TaffyCuckooFilter(n);
  }
}
