package com.github.jbapple.libfilter;

public class TaffyCuckooFilterFactory
    implements FilterFromBytesFactory<TaffyCuckooFilter> {
  public TaffyCuckooFilter CreateFromBytes(int n) { return new TaffyCuckooFilter(n); }
}
