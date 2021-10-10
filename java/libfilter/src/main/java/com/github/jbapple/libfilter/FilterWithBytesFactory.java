package com.github.jbapple.libfilter;

public interface FilterWithBytesFactory<T extends Filter> {
  public T CreateWithBytes(int n);
}
