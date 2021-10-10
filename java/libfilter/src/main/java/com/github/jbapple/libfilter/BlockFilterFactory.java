package com.github.jbapple.libfilter;

public class BlockFilterFactory implements FilterWithBytesFactory<BlockFilter> {
  @Override
  public BlockFilter CreateWithBytes(int n) {
    return new BlockFilter(n);
  }
}
