from cffi import FFI
ffibuilder = FFI()

ffibuilder.set_source("libfilter",
                      r"""
                      #include "filter/block.h"
                      #include "filter/taffy-block.h"
                      #include "filter/taffy-cuckoo.h"
                      """,
                      include_dirs=["../c/include"],
                      library_dirs=["../c/lib"],
                      libraries=["filter"],
                      extra_compile_args=["-march=native"])

ffibuilder.cdef("""
typedef struct {
  uint32_t* block;
  void* to_free;
} libfilter_region;

typedef struct libfilter_block_struct {
  uint64_t num_buckets_;
  libfilter_region block_;
} libfilter_block;

uint64_t libfilter_block_bytes_needed(double ndv, double fpp);
int libfilter_block_init(uint64_t heap_space, libfilter_block *);
int libfilter_block_destruct(libfilter_block *);
inline void libfilter_block_add_hash(uint64_t hash, libfilter_block *);
inline bool libfilter_block_find_hash(uint64_t hash, const libfilter_block *);
""")

if __name__ == "__main__":
  ffibuilder.compile()
