package com.github.jbapple.libfilter;

import java.lang.Math;
import java.util.ArrayList;
import java.util.Arrays;

// TODO: Comparable, Serializable
public class FrozenTaffyCuckooFilter implements StaticFilter /* Clone */ {
  private int log_side_size;
  private byte[] sides[];
  private Feistel feistels[];
  private ArrayList<ArrayList<TaffyCuckooFilter.Path>> stashes;

  void fromSide(TaffyCuckooFilter.Side side, int s) {
    for (int i = 0; i + 3 < side.data.length; i += 4) {
      int j = 5 * (i / 4);
      sides[s][j + 0]  = (byte)(TaffyCuckooFilter.fingerprint(side.data[i + 0]) << 0);
      sides[s][j + 1]  = (byte)(TaffyCuckooFilter.fingerprint(side.data[i + 0]) >> 8);
      sides[s][j + 1] |= (byte)(TaffyCuckooFilter.fingerprint(side.data[i + 1]) << 2);
      sides[s][j + 2]  = (byte)(TaffyCuckooFilter.fingerprint(side.data[i + 1]) >> 6);
      sides[s][j + 2] |= (byte)(TaffyCuckooFilter.fingerprint(side.data[i + 2]) << 4);
      sides[s][j + 3]  = (byte)(TaffyCuckooFilter.fingerprint(side.data[i + 2]) >> 4);
      sides[s][j + 3] |= (byte)(TaffyCuckooFilter.fingerprint(side.data[i + 3]) << 6);
      sides[s][j + 4]  = (byte)(TaffyCuckooFilter.fingerprint(side.data[i + 3]) >> 2);
      // for (int k = 0; k < 4; ++k) {
      //   System.out.print(String.format("0x%08X", TaffyCuckooFilter.fingerprint(side.data[i + k])));
      // }
      // System.out.println("");
      // for (int k = 0; k < 5; ++k) {
      //   System.out.print(String.format("0x%08X", sides[s][j+k]));
      // }
      // System.out.println("");
    }
    feistels[s] = side.f.clone();
    stashes.set(s, new ArrayList<TaffyCuckooFilter.Path>(side.stash));
  }

  FrozenTaffyCuckooFilter(TaffyCuckooFilter tcf) {
    sides = new byte[2][5 << tcf.log_side_size];
    feistels = new Feistel[2];
    stashes = new ArrayList<ArrayList<TaffyCuckooFilter.Path>>();
    stashes.add(null);
    stashes.add(null);
    fromSide(tcf.sides[0], 0);
    fromSide(tcf.sides[1], 1);
    log_side_size = tcf.log_side_size;
  }

  private boolean FindFingerprintInBytes(short fingerprint, int index, int side) {
    byte[] data = sides[side];
    fingerprint = TaffyCuckooFilter.fingerprint(fingerprint);
    // System.out.println(String.format("A 0x%08X", fingerprint));
    // System.out.println(String.format("B1 0x%08X",
    //     ((((short) data[index]) & 0xff) | (((short) (data[index + 1] & 0x3)) << 8))));
    // System.out.println(String.format("B2 0x%08X",
    //          (((((short) data[index + 1]) & 0xff) >> 2)
    //           | (((short) (data[index + 2] & 0xf)) << 6))));
    // System.out.println(String.format("B3 0x%08X",
    //     (((((short) data[index + 2]) & 0xff) >> 4)
    //         | (((short) (data[index + 3] & 0x3f)) << 4))));
    // System.out.println(String.format("B4 0x%08X",
    //     (((((short) data[index + 3]) & 0xff) >> 6)
    //         | ((((short) data[index + 4]) & 0xff) << 2))));
    return (fingerprint
               == ((((short) data[index]) & 0xff)
                   | (((short) (data[index + 1] & 0x3)) << 8)))
        || (fingerprint
            == (((((short) data[index + 1]) & 0xff) >> 2)
                | (((short) (data[index + 2] & 0xf)) << 6)))
        || (fingerprint
            == (((((short) data[index + 2]) & 0xff) >> 4)
                | (((short) (data[index + 3] & 0x3f)) << 4)))
        || (fingerprint
            == (((((short) data[index + 3]) & 0xff) >> 6)
                | ((((short) data[index + 4]) & 0xff) << 2)));
  }

  public boolean FindHash32(int k) {
    long l = k;
    l <<= 32;
    l |= (k * 0x05c2c3e0ffb449c7L) >>> 32;
    return FindHash64(l);
  }

  public boolean FindHash64(long k) {
    for (int s = 0; s < 2; ++s) {
      TaffyCuckooFilter.Path p = TaffyCuckooFilter.toPath(k, feistels[s], log_side_size);
      if (0 == TaffyCuckooFilter.fingerprint(p.slot)) return true;
      for (TaffyCuckooFilter.Path path : stashes.get(s)) {
        if (p.bucket == path.bucket
            && TaffyCuckooFilter.fingerprint(p.slot)
                == TaffyCuckooFilter.fingerprint(path.slot)) {
          return true;
        }
      }
      // System.out.println(String.format("0x%08X", TaffyCuckooFilter.fingerprint(p.slot)));
      // System.out.println(p.bucket);
      if (FindFingerprintInBytes(p.slot, p.bucket * 5, s)) return true;
    }
    return false;
  }
}
