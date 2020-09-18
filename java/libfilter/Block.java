package libfilter;

import java.io.File;
import java.nio.*;

public class Block {
  private ByteBuffer memory;
  static {
    try {
      System.loadLibrary("jni-bridge");
    } catch (UnsatisfiedLinkError e) {
      File f = new File(Block.class.getClassLoader().getResource("libfilter").getPath()
          + "/" + System.mapLibraryName("jni-bridge"));
      System.load(f.getAbsolutePath());
    }
  }
  private native void doNothing();
  private native boolean Allocate(ByteBuffer bb, int bytes);
  private native boolean FindDetail(ByteBuffer bb, long hashval);
  public boolean Find(long hashval) { return FindDetail(memory, hashval); }
  private native void AddDetail(ByteBuffer bb, long hashval);
  public void Add(long hashval) { AddDetail(memory, hashval); }
  private native boolean Deallocate(ByteBuffer bb);

  private native long SizeInBytesDetail(ByteBuffer bb);
  public long SizeInBytes() { return SizeInBytesDetail(memory); }

  private native boolean CloneDetail(ByteBuffer near, ByteBuffer far);
  public Block clone() {
    Block result = new Block();
    CloneDetail(memory, result.memory);
    return result;
  }
  
  // equals
  // clone
  // hashCode
  // toString
  //   static double FalsePositiveProbability(uint64_t ndv, uint64_t bytes) {
  //   return libfilter_block_fpp(ndv, bytes);
  // }

  // static uint64_t MinSpaceNeeded(uint64_t ndv, double fpp) {
  //   return libfilter_block_bytes_needed(ndv, fpp);
  // }

  // static uint64_t MaxCapacity(uint64_t bytes, double fpp) {
  //   return libfilter_block_capacity(bytes, fpp);
  // }

  //   static GenericBF CreateWithNdvFpp(uint64_t ndv, double fpp) {
  //   const uint64_t bytes = libfilter_block_bytes_needed(ndv, fpp);
  //   return CreateWithBytes(bytes);
  // }

  
  public boolean someLibraryMethod() { return true; }
  public Block(int bytes) {
    this.memory = ByteBuffer.allocateDirect(24);
    Allocate(memory, bytes);
  }
  private Block() {
    this.memory = ByteBuffer.allocateDirect(24);
  }
  public void finalize() {
    Deallocate(memory);
  }
}
