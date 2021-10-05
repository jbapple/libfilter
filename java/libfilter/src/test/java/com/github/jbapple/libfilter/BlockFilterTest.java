package com.github.jbapple.libfilter;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.github.jbapple.libfilter.BlockFilter;
import com.github.jbapple.libfilter.TaffyBlockFilter;

import org.junit.Test;

import java.util.ArrayList;
import java.util.Random;
import java.util.concurrent.ThreadLocalRandom;

public class BlockFilterTest {
  public BlockFilterTest() {}

  @Test
  public void InsertPersists() {
    InsertPersistsHelp(new TaffyCuckooFilterFactory());
  }

  public <U extends Filter, T extends FilterFromBytesFactory<U>> void InsertPersistsHelp(
      T factory) {
    int ndv = 16000;
    U x = factory.CreateFromBytes(1);
    ArrayList<Long> hashes = new ArrayList<Long>(ndv);
    Random r = new Random(0xdeadbeef);
    for (int i = 0; i < ndv; ++i) {
      hashes.add(r.nextLong());
    }
    for (int i = 0; i < ndv; ++i) {
      x.AddHash64(hashes.get(i));
      for (int j = 0; j <= i; ++j) {
        //System.out.println("testing " + j + " of " + i);
        assertTrue(x.FindHash64(hashes.get(j)));
      }
    }
  }

  @Test
  public void StartEmpty() {
    int ndv = 16000000;
    TaffyBlockFilter x = TaffyBlockFilter.CreateWithNdvFpp(0, 0.001);
    ThreadLocalRandom r = ThreadLocalRandom.current();
    for (int j = 0; j < ndv; ++j) {
      long v = r.nextLong();
      assertFalse(x.FindHash64(v));
    }
  }
}
