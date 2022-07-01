#include <stdalign.h>

#include "peel.h"
#include "filter/static.h"
#include "memory-internal.h"

// Free result with libfilter_do_free(result.region_, result.length_, 1);
libfilter_static libfilter_static_construct(size_t n, const uint64_t* hashes) {
  size_t size = 1.23 * n;
 retry:;
   const uint64_t alloc_size = libfilter_new_alloc_request(
       size + sizeof(libfilter_static), alignof(libfilter_static));
   libfilter_region_alloc_result region_result =
       libfilter_alloc_at_most(alloc_size, alignof(libfilter_static));
   if (!region_result.zero_filled) libfilter_clear_region(&region_result.region);
   libfilter_static result;
   result.region_ = region_result.region;
   result.length_ = size;

   libfilter_region_alloc_result edge_region_result = libfilter_alloc_at_most(
       libfilter_new_alloc_request(n * sizeof(libfilter_edge), alignof(libfilter_edge)),
       alignof(libfilter_edge));
   libfilter_edge* edges = (libfilter_edge*)edge_region_result.region.block;
   libfilter_init_edges(n, size, hashes, edges);
   libfilter_region_alloc_result nodes_region_result = libfilter_alloc_at_most(
       libfilter_new_alloc_request(size * sizeof(libfilter_peel_node),
                                   alignof(libfilter_peel_node)),
       alignof(libfilter_peel_node));
   if (!nodes_region_result.zero_filled)
     libfilter_clear_region(&nodes_region_result.region);
   libfilter_peel_node* nodes = (libfilter_peel_node*)(nodes_region_result.region.block);
   libfilter_region_alloc_result peel_results_region_result = libfilter_alloc_at_most(
       libfilter_new_alloc_request(size * sizeof(libfilter_edge_peel),
                                   alignof(libfilter_edge_peel)),
       alignof(libfilter_edge_peel));
   libfilter_edge_peel* peel_result =
       (libfilter_edge_peel*)peel_results_region_result.region.block;
   size_t answer = libfilter_peel(size, edges, nodes, peel_result);
   libfilter_do_free(nodes_region_result.region, size * sizeof(libfilter_peel_node),
                     alignof(libfilter_peel_node));
   if (answer < size) {
     ++size;
     libfilter_do_free(peel_results_region_result.region,
                       size * sizeof(libfilter_edge_peel), alignof(libfilter_edge_peel));
     libfilter_do_free(edge_region_result.region, n * sizeof(libfilter_edge),
                       alignof(libfilter_edge));
     libfilter_do_free(region_result.region, alloc_size, alignof(libfilter_static));
     goto retry;
  }
  libfilter_unpeel(size, edges, peel_result, (uint8_t*)result.region_.block);
  libfilter_do_free(peel_results_region_result.region, size * sizeof(libfilter_edge_peel),
                    alignof(libfilter_edge_peel));
  libfilter_do_free(edge_region_result.region, n * sizeof(libfilter_edge),
                    alignof(libfilter_edge));
  return result;
}
