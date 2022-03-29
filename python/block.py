from libfilter import ffi, lib

class Block:
  def __init__(self, ndv, fpp):
    size = lib.libfilter_block_bytes_needed(ndv, fpp)
    self.b = ffi.new("libfilter_block *")
    lib.libfilter_block_init(size, self.b)
    self.b = ffi.gc(self.b, lib.libfilter_block_destruct, size)

  def add(self, hash):
    lib.libfilter_block_add_hash(hash, self.b)

  def find(self, hash):
    return lib.libfilter_block_find_hash(hash, self.b)
