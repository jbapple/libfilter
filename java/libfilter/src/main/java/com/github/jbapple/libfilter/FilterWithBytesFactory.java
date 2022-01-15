package com.github.jbapple.libfilter;

public interface FilterWithBytesFactory<T extends Filter> {
  T CreateWithBytes(int n);
}
