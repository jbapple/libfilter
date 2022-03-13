package com.github.jbapple.libfilter;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import java.util.ArrayList;
import java.util.Random;
import java.util.concurrent.ThreadLocalRandom;

public class FilterTest {
  public FilterTest() {}

  @Test
  public void InsertPersists() {
    InsertPersistsHelp(TaffyCuckooFilter.CreateWithBytes(1));
    InsertPersistsHelp(BlockFilter.CreateWithBytes(32000));
    InsertPersistsHelp(TaffyBlockFilter.CreateWithNdvFpp(1, 0.001));
  }

  @Test
  public void Freeze() {
    TaffyCuckooFilter tcf = TaffyCuckooFilter.CreateWithBytes(1);
    int ndv = 2345678;
    ArrayList<Long> hashes = new ArrayList<Long>(ndv);
    Random r = new Random(0xdeadbeef);
    for (int i = 0; i < ndv; ++i) {
      hashes.add(r.nextLong());
      tcf.AddHash64(hashes.get(i));
    }
    FrozenTaffyCuckooFilter ftcf = new FrozenTaffyCuckooFilter(tcf);
    for (int i = 0; i < ndv; ++i) {
      // System.out.println(String.format("ndv %d", i));
      //assertTrue(tcf.FindHash64(hashes.get(i)));
      assertTrue(ftcf.FindHash64(hashes.get(i)));
    }
  }

  public <T extends Filter> void InsertPersistsHelp(T x) {
    int ndv = 8000;
    ArrayList<Long> hashes = new ArrayList<Long>(ndv);
    Random r = new Random(0xdeadbeef);
    for (int i = 0; i < ndv; ++i) {
      hashes.add(r.nextLong());
    }
    for (int i = 0; i < ndv; ++i) {
      x.AddHash64(hashes.get(i));
      for (int j = 0; j <= i; ++j) {
        assertTrue(x.FindHash64(hashes.get(j)));
      }
    }
  }

  @Test
  public void StartEmptyHelp() {
    StartEmptyHelp(TaffyCuckooFilter.CreateWithBytes(1));
    StartEmptyHelp(BlockFilter.CreateWithBytes(32000));
    StartEmptyHelp(TaffyBlockFilter.CreateWithNdvFpp(1, 0.001));
  }

  public <T extends Filter> void StartEmptyHelp(T x) {
    int ndv = 16000000;
    ThreadLocalRandom r = ThreadLocalRandom.current();
    for (int j = 0; j < ndv; ++j) {
      long v = r.nextLong();
      assertFalse(x.FindHash64(v));
    }
  }
}
