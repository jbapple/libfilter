#include "filter/static.h"

#include <assert.h>
#include <stdalign.h>
#include <string.h>

#include "memory-internal.h"
#include "peel.h"

// Free result with libfilter_do_free(result.region_, result.length_, sizeof(void*));
libfilter_static libfilter_static_construct(size_t n, const uint64_t* hashes) {
  size_t size = 1.23 * n;
  size = (size + sizeof(void*) - 1) / sizeof(void*) * sizeof(void*);
 retry:;
   // initialize the fingerprints
   size_t alloc_size = libfilter_new_alloc_request(size, sizeof(void*));
   libfilter_region_alloc_result region_result =
       libfilter_alloc_at_most(alloc_size, sizeof(void*));
   assert(region_result.block_bytes >= size);
   size = region_result.block_bytes;
   if (!region_result.zero_filled) {
     memset(region_result.region.block, 0, size);
   }
   libfilter_static result;
   result.region_ = region_result.region;
   result.length_ = size;

   // initialize the edges
   libfilter_region_alloc_result edge_region_result = libfilter_alloc_at_most(
       libfilter_new_alloc_request(n * sizeof(libfilter_edge), alignof(libfilter_edge)),
       alignof(libfilter_edge));
   libfilter_edge* edges = (libfilter_edge*)edge_region_result.region.block;
   libfilter_init_edges(n, size, hashes, edges);

   // initialize the nodes
   libfilter_region_alloc_result nodes_region_result = libfilter_alloc_at_most(
       libfilter_new_alloc_request(size * sizeof(libfilter_peel_node),
                                   alignof(libfilter_peel_node)),
       alignof(libfilter_peel_node));
   assert(nodes_region_result.block_bytes == size * sizeof(libfilter_peel_node));
   if (!nodes_region_result.zero_filled) {
     memset(nodes_region_result.region.block, 0, size * sizeof(libfilter_peel_node));
   }
   libfilter_peel_node* nodes = (libfilter_peel_node*)(nodes_region_result.region.block);
   libfilter_populate_peel_nodes(n, edges, nodes);

   // initialize the peel results
   libfilter_region_alloc_result peel_results_region_result = libfilter_alloc_at_most(
       libfilter_new_alloc_request(size * sizeof(libfilter_edge_peel),
                                   alignof(libfilter_edge_peel)),
       alignof(libfilter_edge_peel));
   libfilter_edge_peel* peel_result =
       (libfilter_edge_peel*)peel_results_region_result.region.block;

   // Do the peeling
   size_t answer = libfilter_peel(size, edges, nodes, peel_result);
   libfilter_do_free(nodes_region_result.region, size * sizeof(libfilter_peel_node),
                     alignof(libfilter_peel_node));
   if (answer < size) { // if peeling failed to peel all the way (found a 2-core)
     size += sizeof(void*);
     libfilter_do_free(peel_results_region_result.region,
                       size * sizeof(libfilter_edge_peel), alignof(libfilter_edge_peel));
     libfilter_do_free(edge_region_result.region, n * sizeof(libfilter_edge),
                       alignof(libfilter_edge));
     libfilter_do_free(result.region_, result.length_, sizeof(void*));
     goto retry;
  }
  // populate the fingerprints
  libfilter_unpeel(size, edges, peel_result, (uint8_t*)result.region_.block);
  libfilter_do_free(peel_results_region_result.region, size * sizeof(libfilter_edge_peel),
                    alignof(libfilter_edge_peel));
  libfilter_do_free(edge_region_result.region, n * sizeof(libfilter_edge),
                    alignof(libfilter_edge));
  return result;
}

// Free result with libfilter_do_free(result.region_, result.length_, 1);
void libfilter_static_destruct(libfilter_static input) {
  libfilter_do_free(input.region_, input.length_, sizeof(void*));
}

libfilter_static libfilter_static_clone(libfilter_static filter) {
  libfilter_static result;
  result.length_ = filter.length_;
  result.region_ =
      libfilter_alloc_at_most(libfilter_new_alloc_request(filter.length_, sizeof(void*)),
                              sizeof(void*))
          .region;
  memcpy(result.region_.block, filter.region_.block, filter.length_);
  return result;
}
