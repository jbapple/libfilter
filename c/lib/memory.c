#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "filter/memory.h"
#include "memory-internal.h"

// TODO: try _mm_malloc, __mingw_aligned_malloc, _aligned_malloc

// When C11 is available, aligned_alloc can be used
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define ALIGNED_ALLOC
#elif (defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) || \
    (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 600)
// posix_memalign is a second option and less portable
#define POSIX_MEMALIGN
#else
// Otherwise, we will have to use only a subset of the allocated memory, since the
// allocated memory might not be aligned.
#define UNALIGNED
#endif

// TODO: Makefile testing with and without linux
#if defined(__GLIBC__) && defined(__GLIBC_MINOR__) &&                      \
    (((defined(_BSD_SOURCE) || defined(_SVID_SOURCE)) && __GLIBC__ == 2 && \
      __GLIBC_MINOR__ <= 19) ||                                            \
     (defined(_DEFAULT_SOURCE) &&                                          \
      ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 19))))

#define MMAP
#include <sys/mman.h>

#if defined(__linux__) && __linux__
#define MMAP_ZERO_FILLED true
#else
#define MMAP_ZERO_FILLED false
#endif

static const uint64_t HUGE_PAGE_SIZE = ((uint64_t)1) << 21;

#endif

// TODO: the macro guards might differ between the header and this translation unit

bool __attribute__((visibility("hidden"))) libfilter_alignment_ok(uint64_t alignment) {
  if (alignment < sizeof(void*)) return false;
  return 0 == ((alignment) & (alignment - (uint64_t)1));
}

// returns a value less than or equal to max_bytes. The returned value is the largest
// multiple of alignment that is less than or equal to max_bytes, and so is always greater
// than or equal to max_bytes + 1 - alignment and less than or equal to max_bytes.
uint64_t __attribute__((visibility("hidden")))
libfilter_truncate(uint64_t max_bytes, uint64_t alignment) {
  assert(libfilter_alignment_ok(alignment));
  return max_bytes & (~(alignment - (uint64_t)1));
}

// returns the largest amount of space within max_bytes such that there is always a region
// of that size that both starts and ends at a location that is a multiple of alignment.
//
// The return value is always less than or equal to max_bytes and greater than or equal to
// max_bytes + 2 - 2 * alignment.
#if defined(UNALIGNED) || defined(MMAP)
uint64_t __attribute__((visibility("hidden")))
libfilter_guarantee(uint64_t max_bytes, uint64_t alignment) {
#ifdef UNALIGNED
  return libfilter_truncate(max_bytes + 1 - alignment, alignment);
#else
  return libfilter_truncate(max_bytes, alignment);
#endif
}
#endif

// How many bytes should be requested to guarantee at least exact_bytes of space is
// available
uint64_t __attribute__((visibility("hidden")))
libfilter_new_alloc_request(uint64_t exact_bytes, uint64_t alignment) {
#ifdef MMAP
  if (0 == (exact_bytes & (HUGE_PAGE_SIZE - 1))) return exact_bytes;
#endif
#ifdef UNALIGNED
  return exact_bytes + 2 * alignment - 2;
#else
  (void)alignment;
  return exact_bytes;
#endif
}

#ifdef MMAP
// returns true if using mmap and huge pages would not allocate more than would otherwise
// be allocated by using the next best alternative.
__attribute__((visibility("hidden"))) bool libfilter_mmappable(uint64_t max_bytes,
                                                               uint64_t alignment) {
  if (!libfilter_alignment_ok(alignment)) return false;
  if (alignment > HUGE_PAGE_SIZE) return false;
  const uint64_t guarantee_alignment = libfilter_guarantee(max_bytes, alignment);
  const uint64_t truncate_hugepage = libfilter_truncate(max_bytes, HUGE_PAGE_SIZE);
  return truncate_hugepage > 0 && truncate_hugepage >= guarantee_alignment;
}
#endif

#ifdef MMAP
// allocates a region using mmap
__attribute__((visibility("hidden"))) libfilter_region_alloc_result
libfilter_do_mmap_alloc(uint64_t exact_bytes) {
  assert(exact_bytes > 0);
  // fprintf(stderr, "mmap alloc %ld\n", (long)exact_bytes);
  libfilter_region_alloc_result result = {
      .region = {.block = NULL}, .block_bytes = 0, .zero_filled = MMAP_ZERO_FILLED};
  // printf("mmap 0x%016zx\n", exact_bytes);
  result.region.block = mmap(NULL, exact_bytes, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_HUGETLB | MAP_ANONYMOUS, -1, 0);
  if (MAP_FAILED == result.region.block) {
    result.region.block = NULL;
    result.block_bytes = 0;
  }
  result.block_bytes = exact_bytes;
  result.region.to_free = result.region.block;
  return result;
}
#endif

libfilter_region_alloc_result __attribute__((visibility("hidden")))
libfilter_alloc_at_most(uint64_t max_bytes, uint64_t alignment) {
  assert(libfilter_alignment_ok(alignment));
#ifdef MMAP
  if (libfilter_mmappable(max_bytes, alignment)) {
    return libfilter_do_mmap_alloc(libfilter_truncate(max_bytes, HUGE_PAGE_SIZE));
  }
#endif
  libfilter_region_alloc_result result = {
      .region = {.block = NULL}, .block_bytes = 0, .zero_filled = false};
#ifdef UNALIGNED
  // printf("malloc 0x%016zx\n", guarantee(max_bytes, alignment) + alignment - 1);
  result.region.to_free =
      malloc(libfilter_guarantee(max_bytes, alignment) + alignment - 1);
  if (NULL == result.region.to_free) {
    result.block_bytes = 0;
    return result;
  }
  result.region.block = (void*)((((uintptr_t)result.region.to_free) + alignment - 1) &
                                ~(alignment - (uint64_t)1));
  result.block_bytes =
      libfilter_guarantee(max_bytes, alignment) + alignment - 1 -
      ((uintptr_t)result.region.block - (uintptr_t)result.region.to_free);
  return result;
#else
  result.zero_filled = false;
  result.block_bytes = libfilter_truncate(max_bytes, alignment);
  bool failed_alloc = false;
#ifdef ALIGNED_ALLOC
  // printf("aligned_alloc 0x%016zx\n", truncate(max_bytes, alignment));
  result.region.block =
      aligned_alloc(alignment, result.block_bytes);  // libfilter_truncate(max_bytes, alignment));
  failed_alloc = result.region.block == NULL;
#else
  // printf("posix_memalign 0x%016zx\n", truncate(max_bytes, alignment));
  failed_alloc = 0 != posix_memalign(&result.region.block, alignment,
                                     result.block_bytes);
#endif // ALIGNED_ALLOC
  if (failed_alloc) {
    result.region.block = NULL;
    result.block_bytes = 0;
  }
  result.region.to_free = result.region.block;
  return result;
#endif // UNALIGNED's else
}

#ifdef MMAP
int __attribute__((visibility("hidden")))
libfilter_do_unmap(libfilter_region r, size_t bytes) {
  if (r.block == NULL) return 0;
  return munmap(r.block, bytes);
}
#endif

int __attribute__((visibility("hidden")))
libfilter_do_free(libfilter_region r, uint64_t bytes, uint64_t alignment) {
  (void)bytes;
  assert(libfilter_alignment_ok(alignment));
#ifdef MMAP
  if (0 == (bytes & (HUGE_PAGE_SIZE - (uint64_t)1))) {
    return libfilter_do_unmap(r, bytes);
  }
#endif
  free(r.to_free);
  return 0;
}

void __attribute__((visibility("hidden")))
libfilter_clear_region(libfilter_region* here) {
  here->block = NULL;
  here->to_free = NULL;
}
