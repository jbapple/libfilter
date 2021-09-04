package com.github.jbapple.libfilter;

interface Filter {
  public boolean AddHash32(int h);
  public boolean FindHash32(int h);
  public boolean AddHash64(long h);
  public boolean FindHash64(long h);
}
