package com.github.jbapple.libfilter;

import java.lang.Math;
import java.lang.NullPointerException;
import java.util.Arrays;

public class TaffyBlockFilter
    implements Comparable<TaffyBlockFilter>, Cloneable, Filter, Growable {
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
    if (!(there instanceof TaffyBlockFilter)) return false;
    TaffyBlockFilter that = (TaffyBlockFilter) there;
    return compareTo(that) == 0;
  }

  @Override
  public int hashCode() {
    int[] hashes = new int[cursor];
    for (int i = 0; i < cursor; ++i) {
      hashes[i] = levels[i].hashCode();
    }
    return Arrays.hashCode(hashes);
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
      result.append(Arrays.toString(sizes));
      result.append("|");
      result.append(levels[i].toString());
      result.append("|");
    }
    return result.toString();
  }

  private TaffyBlockFilter() {}

  private TaffyBlockFilter(double ndv, double fpp) {
    ndv = Math.max(ndv, 1);
    levels = new BlockFilter[32];
    sizes = new int[32];
    cursor = 0;
    lastNdv = (ndv > Integer.MAX_VALUE) ? Integer.MAX_VALUE : (int)ndv;
    ttl = (ndv > Integer.MAX_VALUE) ? Integer.MAX_VALUE : (int)ndv;
    levels[0] = BlockFilter.CreateWithNdvFpp(ndv, fpp / 1.65);
    ++cursor;
    for (int x = 0; x < 32; ++x) {
      sizes[x] = BlockFilter.BytesNeeded(
          ndv, fpp / Math.pow(cursor + 1, 2) * 6 / Math.pow(3.1415, 2));
      ndv *= 2;
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

  @Override
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
