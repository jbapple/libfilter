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
  public boolean someLibraryMethod() { return true; }
  public Block(int bytes) {
    this.memory = ByteBuffer.allocateDirect(24);
    Allocate(memory, bytes);
  }
  public void finalize() {
    Deallocate(memory);
  }
}
