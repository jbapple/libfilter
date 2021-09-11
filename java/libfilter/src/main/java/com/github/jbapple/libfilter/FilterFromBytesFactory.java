package com.github.jbapple.libfilter;

public interface FilterFromBytesFactory<T extends Filter> {
  public T CreateFromBytes(int n);
}
