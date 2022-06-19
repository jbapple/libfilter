#include "filter/peel.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>

// size_t libfilter_peel_test(size_t n, size_t m) {
// retry:;
//   uint64_t* hashes = malloc(n * sizeof(uint64_t));
//   size_t to_fill = n * sizeof(uint64_t);
//   while (to_fill > 0) {
//     const size_t filled =
//         getrandom(&((char*)hashes)[n * sizeof(uint64_t) - to_fill], to_fill, 0);
//     to_fill = to_fill - filled;
//     printf("filled %lu out of %lu\n", n * sizeof(uint64_t) - to_fill,
//            n * sizeof(uint64_t));
//     // printf("%s\n", strerror(errno));
//   }

//   libfilter_edge* edges = libfilter_init_edges(n, m, hashes);
//   free(hashes);
//   libfilter_peel_node* nodes = libfilter_init_peel_nodes(m);
//   libfilter_populate_peel_nodes(n, edges, nodes);
//   uint64_t* result = malloc(m * sizeof(uint64_t));
//   uint8_t* xors = calloc(m, sizeof(uint8_t));
//   size_t answer =  libfilter_peel(n, m, edges, nodes, result, xors);
//   if (answer < m) {
//     free(xors);
//     free(result);
//     free(nodes);
//     free(edges);
//     goto retry;
//   }
//   for (size_t i = 0; i < n; ++i) {
//     printf("%ld\n", i);
//     assert(libfilter_find_edge(&edges[i], xors));
//   }
//   free(xors);
//   free(result);
//   for (size_t i = 0; i < m; ++i) {
//     free(nodes[i].edges_);
//   }
//   free(nodes);
//   free(edges);
//   return answer;
// }

void libfilter_unpeel_test_one(size_t edge_count, const libfilter_edge* edges,
                               const libfilter_edge_peel* peel_order, uint8_t* xors) {
  libfilter_unpeel(edge_count, edges, peel_order, xors);
  for (size_t i = 0; i < edge_count; ++i) {
    uint8_t xor_remainder = 0;
    for (int k = 0; k < LIBFILTER_EDGE_ARITY; ++k) {
      //if (edges[edge_order[j]].vertex_[k] == edge_last_nodes[j]) continue;
      xor_remainder ^= xors[edges[peel_order[i].edge_number_].vertex_[k]];
    }
    assert(edges[peel_order[i].edge_number_].fingerprint_ == xor_remainder);
  }
}

void libfilter_unpeel_test_0() {
  libfilter_edge e0 = {.vertex_ = {0, 1, 2}, .fingerprint_ = 1};
  libfilter_edge e1 = {.vertex_ = {1, 2, 3}, .fingerprint_ = 2};
  libfilter_edge edges[] = {e0, e1};
  libfilter_edge_peel peel_order[] = {{0, 0}, {1, 3}};
  uint8_t xors[] = {0, 0, 0, 0};
  libfilter_unpeel_test_one(1, edges, peel_order, xors);
  xors[0] = xors[1] = xors[2] = xors[3] = 0;
  libfilter_unpeel_test_one(2, edges, peel_order, xors);
}

void libfilter_unpeel_test_1() {
  libfilter_edge e0 = {.vertex_ = {0, 1, 2}, .fingerprint_ = 1};
  libfilter_edge e1 = {.vertex_ = {1, 2, 3}, .fingerprint_ = 2};
  libfilter_edge e2 = {.vertex_ = {2, 3, 4}, .fingerprint_ = 2};
  libfilter_edge edges[] = {e0, e1, e2};
  libfilter_edge_peel peel_order[] = {{0, 0}, {2, 4}, {1, 2}};
  uint8_t xors[] = {0, 0, 0, 0, 0};
  libfilter_unpeel_test_one(1, edges, peel_order, xors);
  xors[0] = xors[1] = xors[2] = xors[3] = xors[4] = 0;
  libfilter_unpeel_test_one(2, edges, peel_order, xors);
  xors[0] = xors[1] = xors[2] = xors[3] = xors[4] = 0;
  libfilter_unpeel_test_one(3, edges, peel_order, xors);
}

void  libfilter_unpeel_test() {
  libfilter_unpeel_test_0();
  // libfilter_unpeel_test_1();
}

int main() {
  // size_t n, m;
  // scanf("%ld", &n);
  // scanf("%ld", &m);

  libfilter_unpeel_test();
  // printf("%ld\n", libfilter_peel_test(n, m));
}
