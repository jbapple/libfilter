package com.github.jbapple.libfilter;

import java.util.ArrayList;
import java.lang.Math;

public class TaffyCuckooFilter implements Filter {
  static final short kHeadSize = 10;
  static final short kTailSize = 5;
  static final int kLogBuckets = 2;
  static final int kBuckets = 1 << kLogBuckets;

  static short withFingerprint(short x, short f) {
    x &= (1 << kTailSize) - 1;
    x |= f << (kTailSize + 1);
    return x;
  }

  static short fingerprint(short x) {
    return (short) Feistel.Mask(kHeadSize, x >>> (kTailSize + 1));
  }

  static short withEncodedTail(short x, short t) {
    x >>>= (kTailSize + 1);
    x <<= (kTailSize + 1);
    x |= t;
    return x;
  }

  static short encodedTail(short x) { return x &= ((1 << (kTailSize + 1)) - 1); }

  static class Path {
    int bucket;
    short slot;
    @Override
    public boolean equals(Object o) {
      if (null == o) return false;
      if (!(o instanceof Path)) return false;
      Path p = (Path)o;
      return bucket == p.bucket && slot == p.slot;
    }
  }

  static Path toPath(long raw, Feistel f, int log_side_size) {
   //System.out.println("ToPathBegin " + String.format("0x%016x", raw) + " "
    //+ String.format("0x%016x", f.Summary()) + " " + log_side_size);
    Path p = new Path();
    long pre_hash_index_and_fp = raw >>> (64 - log_side_size - kHeadSize);
   //System.out.println("toPath 01 " + String.format("0x%016X", pre_hash_index_and_fp));
    long hashed_index_and_fp =
        f.Permute(log_side_size + kHeadSize, pre_hash_index_and_fp);
   //System.out.println("toPath 02 " + String.format("0x%016X", hashed_index_and_fp));
    long index = hashed_index_and_fp >>> kHeadSize;
   //System.out.println("toPath 03 " + String.format("0x%016X", index));
    p.bucket = (int)index;
   //System.out.println("toPath 04 " + String.format("0x%016X", p.bucket));
    //System.out.println("toPath " + p.bucket + " " + (1 << log_side_size));
    p.slot = withFingerprint((short)0, (short)hashed_index_and_fp);
   //System.out.println("toPath 05 " + String.format("0x%016X", p.slot));
    long pre_hash_index_fp_and_tail =
        raw >>> (64 - log_side_size - kHeadSize - kTailSize);
   //System.out.println("toPath 06 " + String.format("0x%016X", pre_hash_index_fp_and_tail));
    short tail = (short)Feistel.Mask(kTailSize, pre_hash_index_fp_and_tail);
   //System.out.println("toPath 07 " + String.format("0x%016X", tail));
    tail *= 2;
    tail += 1;
    p.slot = withEncodedTail(p.slot, tail);
   //System.out.println("ToPathEnd "
    //+ String.format("0x%03x", (p.slot >>> 6) & ((1 << 10) - 1)) + " "
    //  + String.format("0x%02x", p.slot & ((1 << 6) - 1)) + " ");
    return p;
  }

  static long FromPathNoTail(Path p,  Feistel f, int log_side_size) {
    long hashed_index_and_fp = ((long)p.bucket << kHeadSize) | fingerprint(p.slot);
   //System.out.println("reverse input " + String.format("0x%016X", p.bucket));
   //System.out.println("reverse input " + String.format("0x%016X", (long)p.bucket << kHeadSize));
   //System.out.println("reverse input " + String.format("0x%016X", p.slot));
   //System.out.println("reverse input " + String.format("0x%016X", p.slot >> (kTailSize + 1)));
   //System.out.println("reverse input " + String.format("0x%016X", p.slot >>> (kTailSize + 1)));
   //System.out.println("reverse input " + String.format("0x%016X", fingerprint(p.slot)));
   //System.out.println("reverse input " + String.format("0x%016X", (long)fingerprint(p.slot)));
   //System.out.println("reverse input " + String.format("0x%016X", hashed_index_and_fp));
    long pre_hashed_index_and_fp =
        f.ReversePermute(log_side_size + kHeadSize, hashed_index_and_fp);
    long shifted_up = pre_hashed_index_and_fp << (64 - log_side_size - kHeadSize);
    return shifted_up;
  }

  static class Side {
    Feistel f;
    ArrayList<Path> stash;
    short[] data;

    void Print(int log_side_size) {
      for (int i = 0; i < (4 << log_side_size); i +=4) {
        for (int j = 0; j < 4; ++j) {
         //System.out.print(
          //String.format("0x%03x", (data[i + j] >>> 6) & ((1 << 10) - 1)) + " "
          //+ String.format("0x%02x", data[i + j] & ((1 << 6) - 1)) + ", ");
        }
       //System.out.println("");
      }
    }

    Path Insert(Path p, PcgRandom rng) {
      // assert (p.tail != 0);
      // Bucket& b = data[p.bucket];
      for (int i = kBuckets * p.bucket; i < kBuckets * p.bucket + kBuckets; ++i) {
        //System.out.println(kBuckets * p.bucket);
        //System.out.println(data.length);
        if (encodedTail(data[i]) == 0) {
          // empty slot
          data[i] = p.slot;
          p.slot = withEncodedTail(p.slot, (short)0);
          return p;
        }
        if (fingerprint(data[i]) == fingerprint(p.slot)) {
          // already present in the table
          if (Util.IsPrefixOf(encodedTail(data[i]), encodedTail(p.slot))) {
            return p;
          }
          /*
          // This has a negligible effect on fpp
          auto c = Combinable(b[i].tail, p.tail);
          if (c > 0) {
            b[i].tail = c;
            return p;
          }
          */
        }
      }
      // Kick something random and return it
      int i = rng.Get();
      Path result = new Path();
      result.bucket = p.bucket;
      result.slot = data[4 * p.bucket + i];
      data[4 * p.bucket + i] = p.slot;
      return result;
    }

    boolean Find(Path p) {
      // assert(p.tail != 0);
     //System.out.println("Finding "
      //+ String.format("0x%03x", (p.slot >>> 6) & ((1 << 10) - 1)) + " "
      //+ String.format("0x%02x", p.slot & ((1 << 6) - 1)) + " ");

      for (Path path : stash) {
        if (encodedTail(path.slot) != 0 && p.bucket == path.bucket
            && fingerprint(p.slot) == fingerprint(path.slot)
            && Util.IsPrefixOf(encodedTail(path.slot), encodedTail(p.slot))) {
          return true;
        }
        //System.out.println("Not stash " + path.bucket + " " + String.format("0x%04x", path.slot));
      }
      for (int i = 4 * p.bucket; i < 4 * p.bucket + kBuckets; ++i) {
        if (encodedTail(data[i]) == 0) continue;
        if (fingerprint(data[i]) == fingerprint(p.slot)
            && Util.IsPrefixOf(encodedTail(data[i]), encodedTail(p.slot))) {
          return true;
        }
        //System.out.println("Not " + i + " " + String.format("0x%04x", data[i]));
      }
      return false;
    }
  }

  Side[] sides;
  int log_side_size;
  PcgRandom rng; // = {detail::kLogBuckets};
  long occupied = 0;

  void Print() {
   //System.out.println("log_side_size: " + log_side_size);
    sides[0].Print(log_side_size);
    sides[1].Print(log_side_size);
  }

  void swap(TaffyCuckooFilter that) {
    for (int i = 0; i < 2; ++i) {
      Side tmp = sides[i];
      sides[i] = that.sides[i];
      that.sides[i] = tmp;
    }
    {
      int tmp = log_side_size;
      log_side_size = that.log_side_size;
      that.log_side_size = tmp;
    }
    {
      PcgRandom tmp = rng;
      rng = that.rng;
      that.rng = tmp;
    }
    {
      long tmp = occupied;
      occupied = that.occupied;
      that.occupied = tmp;
    }
  }

  TaffyCuckooFilter(int log_side, long[] entropy) {
    rng = new PcgRandom(kLogBuckets);
    log_side_size = log_side;
    sides = new Side[2];
    for (int i = 0; i < 2; ++i) {
      sides[i] = new Side();
      sides[i].f = new Feistel(new long[] {entropy[4 * i + 0], entropy[4 * i + 1],
          entropy[4 * i + 2], entropy[4 * i + 3]});
      sides[i].stash = new ArrayList<Path>();
      sides[i].data = new short[kBuckets << log_side_size];
    }
  }

  public boolean FindHash32(int k) {
    long l = k;
    l <<= 32;
    l |= (k * 0x05c2c3e0ffb449c7L) >>> 32;
    return FindHash64(l);
  }

  public boolean AddHash32(int k) {
    long l = k;
    l <<= 32;
    l |= (k * 0x05c2c3e0ffb449c7L) >>> 32;
    return AddHash64(l);
  }

  TaffyCuckooFilter(int bytes) {
    this((int) Math.max(1.0, Math.log(1.0 * bytes / 2 / kBuckets / 2) / Math.log(2)),
         new long[] {0x2ba7538ee1234073L, 0xfcc3777539b147d6L, 0x6086c563576347e7L,
             0x52eff34ee1764465L, 0x8639cbf57f264867L, 0x5a31ee34f0224ccbL,
             0x07a1cb8140744ee6L, 0xf2296cf6a6524e9fL});
  }

  public boolean FindHash64(long k) {
   //System.out.println("FindRaw " + String.format("0x%16x", k));
    for (int s = 0; s < 2; ++s) {
      if (sides[s].Find(toPath(k, sides[s].f, log_side_size))) return true;
    }
    return false;
  }

  // public boolean FindPaths(Path p[2]) {
  //   for (int s = 0; s < 2; ++s) {
  //     if (sides[s].Find(p[s])) return true;
  //   }
  //   return false;
  // }

  boolean InsertPathTTL(int s, Path p, int ttl) {
    //if (sides[0].stash.tail != 0 && sides[1].stash.tail != 0) return InsertResult::Failed;
    Side[] both = new Side[] {sides[s], sides[1 - s]};
    while (true) {
      for (int i = 0; i < 2; ++i) {
        Path q = new Path();//.clone();
        q.bucket = p.bucket;
        q.slot = p.slot;
       //System.out.println("Pre found empty " + q.bucket + " " + q.slot);
        p = both[i].Insert(p, rng);

        if (encodedTail(p.slot) == 0) {
          // Found an empty slot
         //System.out.println("Found empty " + p.bucket + " " + p.slot + " " + q.bucket + " " + q.slot);
          assert both[i].Find(q);
          ++occupied;
          return true;
        }
        if (p.equals(q)) {
         //System.out.println("Found equal");
          assert both[i].Find(q);
          return true;
        }
       //System.out.println("Evicted " + p.bucket + " " + p.slot);
        short tail = encodedTail(p.slot);
        if (ttl <= 0) {
          // we've run out of room. If there's room in this stash, stash it here. If there
          // is no room in this stash, there must be room in the other, based on the
          // pre-condition for this method.
          both[i].stash.add(p);
          ++occupied;
          assert both[i].Find(q);
          return false;
        }
        assert both[i].Find(q);
        --ttl;
        // translate p to being a path about the right half of the table
        long preimage = FromPathNoTail(p, both[i].f, log_side_size);
       //System.out.println("Preimage: " + String.format("0x%016X", preimage));
        p = toPath(preimage, both[1 - i].f, log_side_size);
        // P's tail is now artificially 0, but it should stay the same as we move from side
        // to side
        p.slot = withEncodedTail(p.slot, tail);
      }
    }
  }

  // This method just increases ttl until insert succeeds.
  // TODO: upsize when insert fails with high enough ttl?
  boolean InsertPath(int s, Path q) {
    int ttl = 32;
   //System.out.println("Inserting "
    //+ String.format("0x%03x", (q.slot >>> 6) & ((1 << 10) - 1)) + " "
    //+ String.format("0x%02x", q.slot & ((1 << 6) - 1)) + " ");
    return InsertPathTTL(s, q, ttl);
  }

  // i is the bucket. s is the side
  void UpsizeHelper(short sl, int i, int s, TaffyCuckooFilter t) {
    if (encodedTail(sl) == 0) return;
    Path p = new Path();
    p.slot = sl;
    p.bucket = i;
    long q = FromPathNoTail(p, sides[s].f, log_side_size);
    if (encodedTail(sl) == 1 << kTailSize) {
      // There are no tail bits left! Insert two values.
      // First, hash to the left side of the larger table.
      p = toPath(q, t.sides[0].f, t.log_side_size);
      // Still no tail! :-)
      p.slot = withEncodedTail(p.slot, encodedTail(sl));
      t.InsertPath(0, p);
      // change the raw value by just one bit: its last
      q |= (1L << (64 - log_side_size - kHeadSize - 1));
      p = toPath(q, t.sides[0].f, t.log_side_size);
      p.slot = withEncodedTail(p.slot, encodedTail(sl));
      t.InsertPath(0, p);
    } else {
      // steal a bit from the tail
      q |= (((long) (((long) encodedTail(sl)) >>> kTailSize))
          << (64 - log_side_size - kHeadSize - 1));
      Path r = toPath(q, t.sides[0].f, t.log_side_size);
      r.slot = withEncodedTail(
          r.slot, (short) (Feistel.Mask(kTailSize + 1, encodedTail(sl) << 1)));
      t.InsertPath(0, r);
    }
  }

  void Upsize() {
   //System.out.println("Stash size " + sides[0].stash.size() + sides[1].stash.size());

    TaffyCuckooFilter t = new TaffyCuckooFilter(1 + log_side_size,
        new long[] {sides[0].f.keys[0], sides[0].f.keys[1], sides[0].f.keys[2],
            sides[0].f.keys[3], sides[1].f.keys[0], sides[1].f.keys[1], sides[1].f.keys[2],
            sides[1].f.keys[3]});

    ArrayList<ArrayList<Path>> stashes = new ArrayList<ArrayList<Path>>();
    stashes.add((ArrayList<Path>)sides[0].stash.clone());
    stashes.add((ArrayList<Path>)sides[1].stash.clone());
   //System.out.println("Stash size " + stashes.get(0).size() + stashes.get(1).size());
    sides[0].stash.clear();
    sides[1].stash.clear();
   //System.out.println("Stash size " + stashes.get(0).size() + stashes.get(1).size());
    for (int s = 0; s < 2; ++s) {
      for (int i = 0; i < stashes.get(s).size(); ++i) {
        Path p = stashes.get(s).get(i);
        UpsizeHelper(p.slot, p.bucket, s, t);
      }
      for (int i = 0; i < (1 << log_side_size); ++i) {
        for (int j = 0; j < kBuckets; ++j) {
          short sl = sides[s].data[kBuckets * i + j];
         //System.out.println("Upsizing " + String.format("0x%03x", (sl >>> 6) & ((1 << 10) - 1)) + " "
          //+ String.format("0x%02x", sl & ((1 << 6) - 1)) + " ");
          UpsizeHelper(sl, i, s, t);
          t.Print();
        }
      }
    }
    // System.out.println(log_side_size);
    // System.out.println(t.log_side_size);
    swap(t);
    // System.out.println(log_side_size);
    // System.out.println(t.log_side_size);
  }

  public int Capacity() { return 8 << log_side_size; }

  public boolean AddHash64(long k) {
   //System.out.println("InsertRaw " + String.format("0x%16x", k));
    // System.out.println("Inserting hash " + String.format("0x%016X", k));
    // 95% is achievable, generally,but give it some room
    while (occupied > 0.90 * Capacity() || occupied + 4 >= Capacity() ||
           sides[0].stash.size() + sides[1].stash.size() > 8) {
      Upsize();
    }
    Path p = toPath(k, sides[0].f, log_side_size);
    long preimage = FromPathNoTail(p, sides[0].f, log_side_size);
   //System.out.println("round-trip 0 " + String.format("0x%016X", preimage));
   //System.out.println("Inserting " + p.bucket + " " + p.slot);
    InsertPath(0, p);
    Path q = toPath(k, sides[1].f, log_side_size);
    preimage = FromPathNoTail(q, sides[0].f, log_side_size);
   //System.out.println("round-trip 1 " + String.format("0x%016X", preimage));
   //System.out.println("Didn't insert " + q.bucket + " " + q.slot);
    //assert sides[0].Find(toPath(k, sides[0].f, log_side_size))
    //    || sides[1].Find(toPath(k, sides[1].f, log_side_size));
    // assert FindHash64(k);
    return true;
  }
}
