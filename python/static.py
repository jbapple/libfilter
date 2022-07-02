from libfilter import ffi, lib

class Static:
  def __init__(self, hashes):
    self.b = lib.libfilter_static_construct(len(hashes), hashes)
    self.b = ffi.gc(self.b, lib.libfilter_static_destroy, (int)(len(hashes) * 1.23))

  def __contains__(self, hash):
    return lib.libfilter_static_lookup(self.b, hash)

  def clone(self):
    result = lib.libfilter_static_clone(self.b, result)
    result = ffi.gc(result, lib.libfilter_static_destruct,
                    lib.libfilter_static_size_in_bytes(result))
    answer = Static()
    answer.b = result
    return answer

  def __deepcopy__(self, memo):
    return self.clone()
