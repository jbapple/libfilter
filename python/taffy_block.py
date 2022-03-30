from libfilter import ffi, lib

class TaffyBlock:
  def __init__(self, ndv = None, fpp = None):
    if ndv is not None and fpp is not None:
      self.b = ffi.new("libfilter_taffy_block *")
      lib.libfilter_taffy_block_init(ndv, fpp, self.b)
      self.b = ffi.gc(self.b, lib.libfilter_taffy_block_destruct,
                      lib.libfilter_block_bytes_needed(ndv, fpp))

  def __iadd__(self, hash):
    lib.libfilter_taffy_block_add_hash(self.b, hash)
    # TODO: size may increase. Increase GC pressure?
    return self

  def __contains__(self, hash):
    return lib.libfilter_taffy_block_find_hash(self.b, hash)

  def clone(self):
    result = ffi.new("libfilter_taffy_block *")
    lib.libfilter_taffy_block_clone(self.b, result)
    result = ffi.gc(result, lib.libfilter_taffy_block_destruct,
                    lib.libfilter_taffy_block_size_in_bytes(result))
    answer = TaffyBlock()
    answer.b = result
    return answer

  def __deepcopy__(self, memo):
    return self.clone()
