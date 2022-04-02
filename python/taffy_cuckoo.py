from libfilter import ffi, lib

class TaffyCuckoo:
  def __init__(self, bytes = None):
    if bytes is not None:
      self.b = ffi.new("libfilter_taffy_cuckoo *")
      lib.libfilter_taffy_cuckoo_init(bytes, self.b)
      self.b = ffi.gc(self.b, lib.libfilter_taffy_cuckoo_destruct, bytes)

  def __iadd__(self, hash):
    lib.libfilter_taffy_cuckoo_add_hash(self.b, hash)
    # TODO: size may increase. Increase GC pressure?
    return self

  def __contains__(self, hash):
    return lib.libfilter_taffy_cuckoo_find_hash(self.b, hash)

  def clone(self):
    result = ffi.new("libfilter_taffy_cuckoo *")
    lib.libfilter_taffy_cuckoo_clone(self.b, result)
    result = ffi.gc(result, lib.libfilter_taffy_cuckoo_destruct,
                    lib.libfilter_taffy_cuckoo_size_in_bytes(result))
    answer = TaffyCuckoo()
    answer.b = result
    return answer

  def __deepcopy__(self, memo):
    return self.clone()

class FrozenTaffyCuckoo:
  def __init__(self, taffy_cuckoo):
    self.b = ffi.new("libfilter_frozen_taffy_cuckoo *")
    lib.libfilter_taffy_cuckoo_freeze_init(taffy_cuckoo.b, self.b)
    self.b = ffi.gc(self.b, lib.libfilter_frozen_taffy_cuckoo_destruct,
                    (int)(lib.libfilter_taffy_cuckoo_size_in_bytes(taffy_cuckoo.b) / 8 * 5))

  def __contains__(self, hash):
    return lib.libfilter_frozen_taffy_cuckoo_find_hash(self.b, hash)
