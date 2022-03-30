from libfilter import ffi, lib

class Block:
  def __init__(self, ndv = None, fpp = None):
    if ndv is not None and fpp is not None:
      size = lib.libfilter_block_bytes_needed(ndv, fpp)
      self.b = ffi.new("libfilter_block *")
      lib.libfilter_block_init(size, self.b)
      self.b = ffi.gc(self.b, lib.libfilter_block_destruct, size)

  def __iadd__(self, hash):
    lib.libfilter_block_add_hash(hash, self.b)
    return self

  def __contains__(self, hash):
    return lib.libfilter_block_find_hash(hash, self.b)

  def clone(self):
    result = ffi.new("libfilter_block *")
    lib.libfilter_block_clone(self.b, result)
    result = ffi.gc(result, lib.libfilter_block_destruct,
                    lib.libfilter_block_size_in_bytes(result))
    answer = Block()
    answer.b = result
    return answer

  def __deepcopy__(self, memo):
    return self.clone()
