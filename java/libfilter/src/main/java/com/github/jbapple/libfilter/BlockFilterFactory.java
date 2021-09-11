package com.github.jbapple.libfilter;

public class BlockFilterFactory implements FilterFromBytesFactory<BlockFilter> {
  public BlockFilter CreateFromBytes(int n) { return new BlockFilter(n); }
}
