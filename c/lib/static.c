#include <stdalign.h>

#include "peel.h"
#include "filter/static.h"
#include "memory-internal.h"

libfilter_static* libfilter_static_construct(size_t n, const uint64_t* hashes) {
  size_t size = 1.23 * n;
 retry:;
   const uint64_t alloc_size = libfilter_new_alloc_request(
       size + sizeof(libfilter_static), alignof(libfilter_static));
   libfilter_region_alloc_result region_result =
       libfilter_alloc_at_most(alloc_size, alignof(libfilter_static));
   if (!region_result.zero_filled) libfilter_clear_region(&region_result.region);
   libfilter_static* result = (libfilter_static*)region_result.region.block;
   result->length_ = size;

   libfilter_edge* edges = libfilter_init_edges(n, size, hashes);
   libfilter_peel_node* nodes = libfilter_init_peel_nodes(size);
   libfilter_populate_peel_nodes(n, edges, nodes);
   libfilter_edge_peel* peel_result = malloc(size * sizeof(libfilter_edge_peel));
   size_t answer = libfilter_peel(size, edges, nodes, peel_result);
   free(nodes);
   if (answer < size) {
     ++size;
     free(peel_result);
     free(edges);
     libfilter_do_free(region_result.region, alloc_size, alignof(libfilter_static));
     goto retry;
  }
  libfilter_unpeel(size, edges, peel_result, result->data_);
  free(peel_result);
  free(edges);
  return result;
}
