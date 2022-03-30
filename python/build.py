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
                      extra_compile_args=["-march=native"],
                      extra_link_args=["-Wl,-rpath=../c/lib"])

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
int libfilter_block_clone(const libfilter_block *, libfilter_block *);
inline uint64_t libfilter_block_size_in_bytes(const libfilter_block *);

typedef struct {
  libfilter_block levels[48];
  uint64_t sizes[48];
  int cursor;
  uint64_t last_ndv;
  int64_t ttl;
} libfilter_taffy_block;

int libfilter_taffy_block_clone(const libfilter_taffy_block* b, libfilter_taffy_block*);
void libfilter_taffy_block_destruct(libfilter_taffy_block* here);
int libfilter_taffy_block_init(uint64_t ndv, double fpp, libfilter_taffy_block*);
inline uint64_t libfilter_taffy_block_size_in_bytes(const libfilter_taffy_block* here);
inline bool libfilter_taffy_block_add_hash(libfilter_taffy_block* here, uint64_t h);
inline bool libfilter_taffy_block_find_hash(const libfilter_taffy_block* here, uint64_t h);

typedef struct {
  uint64_t fingerprint : 10;
  uint64_t tail : 6;
} libfilter_taffy_cuckoo_slot;

typedef struct {
  libfilter_taffy_cuckoo_slot slot;
  uint64_t bucket;
} libfilter_taffy_cuckoo_path;

typedef struct {
  libfilter_taffy_cuckoo_slot data[4];
} libfilter_taffy_cuckoo_bucket;

typedef struct {
  uint64_t keys[2][2];
} libfilter_feistel;

typedef struct {
  libfilter_feistel f;
  libfilter_taffy_cuckoo_bucket* data;
  size_t stash_capacity;
  size_t stash_size;
  libfilter_taffy_cuckoo_path* stash;
} libfilter_taffy_cuckoo_side;

typedef struct {
  int bit_width;
  uint64_t state;
  uint64_t inc;
  uint32_t current;
  int remaining_bits;
} libfilter_pcg_random;

typedef struct libfilter_taffy_cuckoo_struct {
  libfilter_taffy_cuckoo_side sides[2];
  int log_side_size;
  libfilter_pcg_random rng;
  const uint64_t* entropy;
  uint64_t occupied;
} libfilter_taffy_cuckoo;

void libfilter_taffy_cuckoo_swap(libfilter_taffy_cuckoo* x, libfilter_taffy_cuckoo* y);
int libfilter_taffy_cuckoo_clone(const libfilter_taffy_cuckoo*, libfilter_taffy_cuckoo*);
void libfilter_taffy_cuckoo_init(uint64_t bytes, libfilter_taffy_cuckoo* here);
uint64_t libfilter_taffy_cuckoo_size_in_bytes(const libfilter_taffy_cuckoo* here);
inline bool libfilter_taffy_cuckoo_find_hash(const libfilter_taffy_cuckoo* here, uint64_t k);
void libfilter_taffy_cuckoo_destruct(libfilter_taffy_cuckoo* t);
inline bool libfilter_taffy_cuckoo_add_hash(libfilter_taffy_cuckoo* here, uint64_t k);

typedef struct {
  uint64_t zero : 10;
  uint64_t one : 10;
  uint64_t two : 10;
  uint64_t three : 10;
} libfilter_frozen_taffy_cuckoo_bucket;

typedef struct libfilter_frozen_taffy_cuckoo_struct {
  libfilter_feistel hash_[2];
  int log_side_size_;
  libfilter_frozen_taffy_cuckoo_bucket* data_[2];
  uint64_t* stash_[2];
  size_t stash_capacity_[2];
  size_t stash_size_[2];
} libfilter_frozen_taffy_cuckoo;

void libfilter_taffy_cuckoo_freeze_init(const libfilter_taffy_cuckoo*, libfilter_frozen_taffy_cuckoo*);
size_t libfilter_frozen_taffy_cuckoo_size_in_bytes(const libfilter_frozen_taffy_cuckoo*);
inline bool libfilter_frozen_taffy_cuckoo_find_hash(const libfilter_frozen_taffy_cuckoo*, uint64_t);
void libfilter_frozen_taffy_cuckoo_destruct(libfilter_frozen_taffy_cuckoo*);
""")

if __name__ == "__main__":
  ffibuilder.compile()
