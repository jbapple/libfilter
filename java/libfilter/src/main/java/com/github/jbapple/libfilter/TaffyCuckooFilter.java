package com.github.jbapple.libfilter;

import java.lang.Math;
import java.util.ArrayList;
import java.util.Arrays;

// TODO: Comparable, Serialeable
/**
 * TaffyCuckooFilter is a filter based on the paper "Stretching your data with taffy
 * filters". It can grow when it becomes full without affecting the false positive
 * substantially, unlike most other types of filter.
 */
public class TaffyCuckooFilter implements Filter, Cloneable, Growable {
  private static final short kHeadSize = 10;
  private static final short kTailSize = 5;
  private static final int kLogBuckets = 2;
  private static final int kBuckets = 1 << kLogBuckets;

  private static short withFingerprint(short x, short f) {
    x &= (1 << kTailSize) - 1;
    x |= f << (kTailSize + 1);
    return x;
  }

  private static short fingerprint(short x) {
    return (short) Feistel.Mask(kHeadSize, x >>> (kTailSize + 1));
  }

  private static short withEncodedTail(short x, short t) {
    x >>>= (kTailSize + 1);
    x <<= (kTailSize + 1);
    x |= t;
    return x;
  }

  private static short encodedTail(short x) { return x &= ((1 << (kTailSize + 1)) - 1); }

  private static class Path implements Cloneable {
    private int bucket;
    private short slot;

    @Override
    public Path clone() {
      Path result = new Path();
      result.bucket = bucket;
      result.slot = slot;
      return result;
    }

    @Override
    public boolean equals(Object o) {
      if (null == o) return false;
      if (!(o instanceof Path)) return false;
      Path p = (Path)o;
      return bucket == p.bucket && slot == p.slot;
    }

    @Override
    public int hashCode() {
      return bucket ^ slot;
    }
  }

  private static Path toPath(long raw, Feistel f, int log_side_size) {
    Path p = new Path();
    long pre_hash_index_and_fp = raw >>> (64 - log_side_size - kHeadSize);
    long hashed_index_and_fp =
        f.Permute(log_side_size + kHeadSize, pre_hash_index_and_fp);
    long index = hashed_index_and_fp >>> kHeadSize;
    p.bucket = (int)index;
    p.slot = withFingerprint((short)0, (short)hashed_index_and_fp);
    long pre_hash_index_fp_and_tail =
        raw >>> (64 - log_side_size - kHeadSize - kTailSize);
    short tail = (short)Feistel.Mask(kTailSize, pre_hash_index_fp_and_tail);
    tail *= 2;
    tail += 1;
    p.slot = withEncodedTail(p.slot, tail);
    return p;
  }

  private static long FromPathNoTail(Path p,  Feistel f, int log_side_size) {
    long hashed_index_and_fp = ((long)p.bucket << kHeadSize) | fingerprint(p.slot);
    long pre_hashed_index_and_fp =
        f.ReversePermute(log_side_size + kHeadSize, hashed_index_and_fp);
    long shifted_up = pre_hashed_index_and_fp << (64 - log_side_size - kHeadSize);
    return shifted_up;
  }

  private static class Side implements Cloneable {
    private Feistel f;
    private ArrayList<Path> stash;
    private short[] data;

    @Override
    public Side clone() {
      Side result = new Side();
      result.f = f.clone();
      result.stash = stash;
      for (int i = 0; i < stash.size(); ++i) {
        result.stash.set(i, stash.get(i).clone());
      }
      result.data = data.clone();
      return result;
    }

    @Override
    public boolean equals(Object o) {
      if (null == o) return false;
      if (!(o instanceof Side)) return false;
      Side s = (Side) o;
      return f.equals(s.f) && stash.equals(s.stash) && Arrays.equals(data, s.data);
    }

    @Override
    public int hashCode() {
      return f.hashCode() ^ stash.hashCode() ^ Arrays.hashCode(data);
    }

    Path Insert(Path p, PcgRandom rng) {
      for (int i = kBuckets * p.bucket; i < kBuckets * p.bucket + kBuckets; ++i) {
        if (encodedTail(data[i]) == 0) {
          // empty slot
          data[i] = p.slot;
          p.slot = withEncodedTail(p.slot, (short) 0);
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
      for (Path path : stash) {
        if (encodedTail(path.slot) != 0 && p.bucket == path.bucket
            && fingerprint(p.slot) == fingerprint(path.slot)
            && Util.IsPrefixOf(encodedTail(path.slot), encodedTail(p.slot))) {
          return true;
        }
      }
      for (int i = 4 * p.bucket; i < 4 * p.bucket + kBuckets; ++i) {
        if (encodedTail(data[i]) == 0) continue;
        if (fingerprint(data[i]) == fingerprint(p.slot)
            && Util.IsPrefixOf(encodedTail(data[i]), encodedTail(p.slot))) {
          return true;
        }
      }
      return false;
    }
  }

  private Side[] sides;
  private int log_side_size;
  private PcgRandom rng;
  private long occupied = 0;

  /**
   * Swaps the contents of <code>that</code> with <code>this</code>.
   */
  public void Swap(TaffyCuckooFilter that) {
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

  @Override
  public TaffyCuckooFilter clone() {
    TaffyCuckooFilter result = new TaffyCuckooFilter(0);
    result.sides = sides.clone();
    for(int i = 0; i < result.sides.length; ++i) {
      result.sides[i] = sides[i].clone();
    }
    result.log_side_size = log_side_size;
    result.rng = rng.clone();
    result.occupied = occupied;
    return result;
  }

  // TODO: equals and hashCode that respect the semantic contents of a filter, not just
  // the representation.

  /**
   * Creates a filter with a capacity of <code>7 << log_side</code> using
   * <code>entropy</code> for hashing.
   *
   * @param log_side a representation of the size of the filter as a power of 2
   * @param entropy an array containing eight longs, used for hashing. Note that
   *     TaffyCuckooFilters take as input hash values - this entropy is only used
   *     internally to re-hash (when needed) the hash values supplied by the user
   */
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

  /**
   * Creates a TaffyCuckooFilter using approximately the heap space indicated by
   * <code>n</code>.
   *
   * @param n the number of bytes to occupy.
   */
  public static TaffyCuckooFilter CreateWithBytes(int n) {
    return new TaffyCuckooFilter(n);
  }

  public boolean FindHash64(long k) {
    for (int s = 0; s < 2; ++s) {
      if (sides[s].Find(toPath(k, sides[s].f, log_side_size))) return true;
    }
    return false;
  }

  private boolean InsertPathTTL(int s, Path p, int ttl) {
    Side[] both = new Side[] {sides[s], sides[1 - s]};
    while (true) {
      for (int i = 0; i < 2; ++i) {
        Path q = new Path();
        q.bucket = p.bucket;
        q.slot = p.slot;
        p = both[i].Insert(p, rng);
        if (encodedTail(p.slot) == 0) {
          // Found an empty slot
          ++occupied;
          return true;
        }
        if (p.equals(q)) {
          return true;
        }
        short tail = encodedTail(p.slot);
        if (ttl <= 0) {
          // we've run out of room.
          both[i].stash.add(p);
          ++occupied;
          return false;
        }
        --ttl;
        // translate p to being a path about the right half of the table
        long preimage = FromPathNoTail(p, both[i].f, log_side_size);
        p = toPath(preimage, both[1 - i].f, log_side_size);
        // P's tail is now artificially 0, but it should stay the same as we move from side
        // to side
        p.slot = withEncodedTail(p.slot, tail);
      }
    }
  }

  private boolean InsertPath(int s, Path q) {
    int ttl = 32;
    return InsertPathTTL(s, q, ttl);
  }

  // i is the bucket. s is the side
  private void UpsizeHelper(short sl, int i, int s, TaffyCuckooFilter t) {
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

  @Override
  public void Upsize() {
    TaffyCuckooFilter t = new TaffyCuckooFilter(1 + log_side_size,
        new long[] {sides[0].f.keys[0], sides[0].f.keys[1], sides[0].f.keys[2],
            sides[0].f.keys[3], sides[1].f.keys[0], sides[1].f.keys[1], sides[1].f.keys[2],
            sides[1].f.keys[3]});

    ArrayList<ArrayList<Path>> stashes = new ArrayList<ArrayList<Path>>();
    stashes.add((ArrayList<Path>)sides[0].stash.clone());
    stashes.add((ArrayList<Path>)sides[1].stash.clone());
    sides[0].stash.clear();
    sides[1].stash.clear();
    for (int s = 0; s < 2; ++s) {
      for (int i = 0; i < stashes.get(s).size(); ++i) {
        Path p = stashes.get(s).get(i);
        UpsizeHelper(p.slot, p.bucket, s, t);
      }
      for (int i = 0; i < (1 << log_side_size); ++i) {
        for (int j = 0; j < kBuckets; ++j) {
          short sl = sides[s].data[kBuckets * i + j];
          UpsizeHelper(sl, i, s, t);
        }
      }
    }
    Swap(t);
  }

  private int Capacity() { return 8 << log_side_size; }

  public boolean AddHash64(long k) {
    // 95% is achievable, generally,but give it some room
    while (occupied > 0.90 * Capacity() || occupied + 4 >= Capacity() ||
           sides[0].stash.size() + sides[1].stash.size() > 8) {
      Upsize();
    }
    Path p = toPath(k, sides[0].f, log_side_size);
    long preimage = FromPathNoTail(p, sides[0].f, log_side_size);
    InsertPath(0, p);
    Path q = toPath(k, sides[1].f, log_side_size);
    preimage = FromPathNoTail(q, sides[0].f, log_side_size);
    return true;
  }

  // TODO
  // @Override
  // public String toString() {}
}
