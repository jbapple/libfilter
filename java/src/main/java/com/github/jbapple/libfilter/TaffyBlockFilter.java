package com.github.jbapple.libfilter;

import java.lang.Math;
import java.lang.NullPointerException;

public class TaffyBlockFilter implements Comparable<TaffyBlockFilter>, Cloneable, Filter {
  private BlockFilter[] levels;
  private int[] sizes;
  private int cursor;
  private int lastNdv;
  private long ttl;
  private double fpp;

  @Override
  public int compareTo(TaffyBlockFilter o) {
    // TODO: this is not lexicographic
    if (o == null) throw new NullPointerException();
    if (sizes.length < o.sizes.length) return -1;
    if (sizes.length > o.sizes.length) return +1;
    for (int i = 0; i < sizes.length; ++i) {
      int inner = levels[i].compareTo(o.levels[i]);
      if (0 == inner) continue;
      return inner;
    }
    return 0;
  }

  @Override
  public TaffyBlockFilter clone() {
    TaffyBlockFilter result = new TaffyBlockFilter();
    result.levels = levels.clone();
    for (int i = 0; i < cursor; ++i) {
      result.levels[i] = levels[i].clone();
    }
    result.sizes = sizes.clone();
    result.cursor = cursor;
    result.lastNdv = lastNdv;
    result.ttl = ttl;
    result.fpp = fpp;
    return result;
  }

  @Override
  public boolean equals(Object there) {
    if (there == null) return false;
    if (!(there instanceof BlockFilter)) return false;
    TaffyBlockFilter that = (TaffyBlockFilter) there;
    return compareTo(that) == 0;
  }

  @Override
  public int hashCode() {
    int[] hashes = new int[cursor];
    for (int i = 0; i < cursor; ++i) {
      hashes[i] = levels[i].hashCode();
    }
    return hashes.hashCode();
  }

  @Override
  public String toString() {
    StringBuilder result = new StringBuilder();
    result.append(cursor)
        .append("|")
        .append(lastNdv)
        .append("|")
        .append(ttl)
        .append("|")
        .append(fpp)
        .append("|");
    for (int i = 0; i < cursor; ++i) {
      result.append(sizes);
      result.append("|");
      result.append(levels[i].clone());
      result.append("|");
    }
    return result.toString();
  }

  private TaffyBlockFilter() {}

  private TaffyBlockFilter(int ndv, double fpp) {
    levels = new BlockFilter[32];
    sizes = new int[32];
    cursor = 0;
    lastNdv = ndv;
    ttl = ndv;
    fpp = fpp;
    levels[0] = BlockFilter.CreateWithNdvFpp(ndv, fpp / 1.65);
    ++cursor;
    for (int x = 0; x < 32; ++x) {
      sizes[x] = BlockFilter.BytesNeeded(
          ndv << x, fpp / Math.pow(cursor + 1, 2) * 6 / Math.pow(3.1415, 2));
    }
  }

  public static TaffyBlockFilter CreateWithNdvFpp(int ndv, double fpp) {
    return new TaffyBlockFilter(ndv, fpp);
  }

  public long SizeInBytes() {
    long result = 0;
    for (int i = 0; i < cursor; ++i) {
      result += levels[i].sizeInBytes();
    }
    return result;
  }

  public void Upsize() {
    lastNdv *= 2;
    levels[cursor] = BlockFilter.CreateWithBytes(sizes[cursor]);
    ++cursor;
    ttl = lastNdv;
  }

  @Override
  public boolean AddHash32(int h) {
    if (ttl <= 0) Upsize();
    levels[cursor - 1].AddHash32(h);
    --ttl;
    return true;
  }

  @Override
  public boolean FindHash32(int h) {
    for (int i = 0; i < cursor; ++i) {
      if (levels[i].FindHash32(h)) return true;
    }
    return false;
  }

  @Override
  public boolean AddHash64(long h) {
    if (ttl <= 0) Upsize();
    levels[cursor - 1].AddHash64(h);
    --ttl;
    return true;
  }

  @Override
  public boolean FindHash64(long h) {
    for (int i = 0; i < cursor; ++i) {
      if (levels[i].FindHash64(h)) return true;
    }
    return false;
  }
}
