package com.github.jbapple.libfilter;

import java.util.ArrayList;

public class TaffyCuckooFilter {
  static final short kHeadSize = 10;
  static final short kTailSize = 5;
  static final int kLogBuckets = 2;
  static final int kBuckets = 1 << kLogBuckets;

  static short withFingerprint(short x, short f) {
    x &= (1 << kTailSize) - 1;
    x |= f << (kTailSize + 1);
    return x;
  }

  static short fingerprint(short x) { return x >>>= (kTailSize + 1); }

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
  }

  static Path toPath(long raw, Feistel f, int log_side_size) {
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

  static long FromPathNoTail(Path p,  Feistel f, int log_side_size) {
    long hashed_index_and_fp = (p.bucket << kHeadSize) | fingerprint(p.slot);
    long pre_hashed_index_and_fp =
        f.ReversePermute(log_side_size + kHeadSize, hashed_index_and_fp);
    long shifted_up = pre_hashed_index_and_fp << (64 - log_side_size - kHeadSize);
    return shifted_up;
  }

  static class Side {
    Feistel f;
    ArrayList<Path> stash;
    short[] data;

    Path Insert(Path p, PcgRandom rng) {
      // assert (p.tail != 0);
      // Bucket& b = data[p.bucket];
      for (int i = 4 * p.bucket; i < 4 * p.bucket + kBuckets; ++i) {
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
      Path result = p;
      result.slot = data[4 * p.bucket + i];
      data[4 * p.bucket + i] = p.slot;
      return result;
    }

    boolean Find(Path p) {
      // assert(p.tail != 0);
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
}

// class TaffyCuckooFilter {
//   Side sides[2];
//   int log_side_size;
//   PcgRandom rng; // = {detail::kLogBuckets};
//   long occupied = 0;

//     TaffyCuckooFilter(int log_side_size, const uint64_t* entropy)
//       : sides{{log_side_size, entropy}, {log_side_size, entropy + 4}},
//         log_side_size(log_side_size),
//         entropy(entropy) {}

//   TaffyCuckooFilter() {}
