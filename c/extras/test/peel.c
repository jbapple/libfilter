#include "filter/peel.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

size_t libfilter_round_trip_test(size_t n, size_t m) {
retry:;
  uint64_t* hashes = malloc(n * sizeof(uint64_t));
  size_t to_fill = n * sizeof(uint64_t);
#if defined(__APPLE__)
  arc4random_buf(hashes, to_fill);
#elif defined(__linux__)
  while (to_fill > 0) {
    const size_t filled =
        getrandom(&((char*)hashes)[n * sizeof(uint64_t) - to_fill], to_fill, 0);
    to_fill = to_fill - filled;
    printf("filled %lu out of %lu\n", n * sizeof(uint64_t) - to_fill,
           n * sizeof(uint64_t));
    // printf("%s\n", strerror(errno));
  }
#else
#error "TODO: non-unix RNG"
#endif

  libfilter_edge* edges = libfilter_init_edges(n, m, hashes);
  free(hashes);
  libfilter_peel_node* nodes = libfilter_init_peel_nodes(m);
  libfilter_populate_peel_nodes(n, edges, nodes);
  libfilter_edge_peel* result = malloc(m * sizeof(libfilter_edge_peel));
  uint8_t* xors = calloc(m, sizeof(uint8_t));
  size_t answer = libfilter_peel(m, edges, nodes, result);
  printf("peeled %ld\n", answer);
  if (answer < m) {
    free(xors);
    free(result);
    free(nodes);
    free(edges);
    goto retry;
  }
  libfilter_unpeel(m, edges, result, xors);
  for (size_t i = 0; i < n; ++i) {
    //printf("%ld\n", i);
    assert(libfilter_find_edge(&edges[i], xors));
  }
  free(xors);
  free(result);
  free(nodes);
  free(edges);
  return answer;
}

void libfilter_unpeel_test_reverse(size_t edge_count, const libfilter_edge* edges,
                                   const libfilter_edge_peel* peel_order, uint8_t* xors) {
  libfilter_unpeel(edge_count, edges, peel_order, xors);
  for (size_t i = 0; i < edge_count; ++i) {
    assert(libfilter_find_edge(&edges[peel_order[i].edge_number_], xors));
  }
}

void libfilter_peel_once_test_0() {
  libfilter_edge e0 = {.vertex_ = {0, 1, 2}, .fingerprint_ = 1};
  libfilter_edge e1 = {.vertex_ = {1, 2, 3}, .fingerprint_ = 2};
  libfilter_edge edges[] = {e0, e1};
  libfilter_edge_peel peel_order[] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
  libfilter_peel_node peel_nodes[] = {{1, 0}, {2, 1}, {2, 1}, {1, 1}};
  assert(2 == libfilter_peel_once(0, edges, peel_nodes, peel_order));
  assert(peel_order[0].edge_number_ == 1);
  assert(peel_order[0].peeled_vertex_ == 1);
  assert(peel_order[1].edge_number_ == 1);
  assert(peel_order[1].peeled_vertex_ == 2);
  assert(0 == libfilter_peel_once(1, edges, peel_nodes, &peel_order[2]));
  assert(peel_nodes[0].count_ == 0);
  assert(peel_nodes[1].count_ == 0);
  assert(peel_nodes[2].count_ == 0);
  assert(peel_nodes[3].count_ == 0);
}

void libfilter_peel_test_0() {
  libfilter_edge e0 = {.vertex_ = {0, 1, 2}, .fingerprint_ = 1};
  libfilter_edge e1 = {.vertex_ = {1, 2, 3}, .fingerprint_ = 2};
  const libfilter_edge edges[] = {e0, e1};
  libfilter_edge_peel peel_order[] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
  libfilter_peel_node peel_nodes[] = {{1, 0}, {2, 1}, {2, 1}, {1, 1}};
  assert(4 == libfilter_peel(4, edges, peel_nodes, peel_order));
  assert(peel_order[0].edge_number_ == 0);
  assert(peel_order[0].peeled_vertex_ == 0);
  assert(peel_order[1].edge_number_ == 1);
  assert(peel_order[1].peeled_vertex_ == 3);
  assert(peel_order[2].edge_number_ == 1);
  assert(peel_order[2].peeled_vertex_ == 1);
  assert(peel_order[3].edge_number_ == 1);
  assert(peel_order[3].peeled_vertex_ == 2);
}

void libfilter_unpeel_test_0() {
  libfilter_edge e0 = {.vertex_ = {0, 1, 2}, .fingerprint_ = 1};
  libfilter_edge e1 = {.vertex_ = {1, 2, 3}, .fingerprint_ = 2};
  libfilter_edge edges[] = {e0, e1};
  libfilter_edge_peel peel_order[] = {{0, 0}, {1, 3}};
  uint8_t xors[] = {0, 0, 0, 0};
  libfilter_unpeel_test_reverse(1, edges, peel_order, xors);
  xors[0] = xors[1] = xors[2] = xors[3] = 0;
  libfilter_unpeel_test_reverse(2, edges, peel_order, xors);
}

void libfilter_unpeel_test_1() {
  libfilter_edge e0 = {.vertex_ = {0, 1, 2}, .fingerprint_ = 1};
  libfilter_edge e1 = {.vertex_ = {1, 2, 3}, .fingerprint_ = 2};
  libfilter_edge e2 = {.vertex_ = {2, 3, 4}, .fingerprint_ = 2};
  libfilter_edge edges[] = {e0, e1, e2};
  libfilter_edge_peel peel_order[] = {{0, 0}, {2, 4}, {1, 2}};
  uint8_t xors[] = {0, 0, 0, 0, 0};
  libfilter_unpeel_test_reverse(1, edges, peel_order, xors);
  xors[0] = xors[1] = xors[2] = xors[3] = xors[4] = 0;
  libfilter_unpeel_test_reverse(2, edges, peel_order, xors);
  xors[0] = xors[1] = xors[2] = xors[3] = xors[4] = 0;
  libfilter_unpeel_test_reverse(3, edges, peel_order, xors);
}

void libfilter_peel_once_test_1() {
  libfilter_edge e0 = {.vertex_ = {0, 1, 2}, .fingerprint_ = 1};
  libfilter_edge e1 = {.vertex_ = {1, 2, 3}, .fingerprint_ = 2};
  libfilter_edge e2 = {.vertex_ = {2, 3, 4}, .fingerprint_ = 2};
  libfilter_edge edges[] = {e0, e1, e2};
  libfilter_edge_peel peel_order[] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
  libfilter_peel_node peel_nodes[] = {
      {1, 0}, {2, 0 ^ 1}, {3, 0 ^ 1 ^ 2}, {2, 1 ^ 2}, {1, 2}};
  assert(1 == libfilter_peel_once(0, edges, peel_nodes, peel_order));
  assert(peel_order[0].edge_number_ == 1);
  assert(peel_order[0].peeled_vertex_ == 1);
  assert(2 == libfilter_peel_once(4, edges, peel_nodes, &peel_order[1]));
  assert(peel_order[1].edge_number_ == 1);
  assert(peel_order[1].peeled_vertex_ == 2);
  assert(peel_order[2].edge_number_ == 1);
  assert(peel_order[2].peeled_vertex_ == 3);
  assert(0 == libfilter_peel_once(1, edges, peel_nodes, &peel_order[3]));

  assert(peel_nodes[0].count_ == 0);
  assert(peel_nodes[1].count_ == 0);
  assert(peel_nodes[2].count_ == 0);
  assert(peel_nodes[3].count_ == 0);
  assert(peel_nodes[4].count_ == 0);
}

void libfilter_peel_test_1() {
  libfilter_edge e0 = {.vertex_ = {0, 1, 2}, .fingerprint_ = 1};
  libfilter_edge e1 = {.vertex_ = {1, 2, 3}, .fingerprint_ = 2};
  libfilter_edge e2 = {.vertex_ = {2, 3, 4}, .fingerprint_ = 2};
  const libfilter_edge edges[] = {e0, e1, e2};
  libfilter_edge_peel peel_order[] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};
  libfilter_peel_node peel_nodes[] = {
      {1, 0}, {2, 0 ^ 1}, {3, 0 ^ 1 ^ 2}, {2, 1 ^ 2}, {1, 2}};
  assert(5 == libfilter_peel(5, edges, peel_nodes, peel_order));
  assert(peel_order[0].edge_number_ == 0);
  assert(peel_order[0].peeled_vertex_ == 0);
  assert(peel_order[1].edge_number_ == 2);
  assert(peel_order[1].peeled_vertex_ == 4);
  assert(peel_order[2].edge_number_ == 1);
  assert(peel_order[2].peeled_vertex_ == 1);
  assert(peel_order[3].edge_number_ == 1);
  assert(peel_order[3].peeled_vertex_ == 2);
  assert(peel_order[4].edge_number_ == 1);
  assert(peel_order[4].peeled_vertex_ == 3);
}

void libfilter_unpeel_test_2() {
  libfilter_edge e0 = {.vertex_ = {0, 1, 2}, .fingerprint_ = 1};
  libfilter_edge e1 = {.vertex_ = {1, 2, 3}, .fingerprint_ = 2};
  libfilter_edge e2 = {.vertex_ = {2, 3, 4}, .fingerprint_ = 2};
  libfilter_edge edges[] = {e0, e1, e2};
  libfilter_edge_peel peel_order[] = {{0, 0}, {2, 4}, {1, 1}, {1, 2}, {1, 3}};
  uint8_t xors[] = {0, 0, 0, 0, 0};
  libfilter_unpeel_test_reverse(1, edges, peel_order, xors);
  xors[0] = xors[1] = xors[2] = xors[3] = xors[4] = 0;
  libfilter_unpeel_test_reverse(2, edges, peel_order, xors);
  xors[0] = xors[1] = xors[2] = xors[3] = xors[4] = 0;
  libfilter_unpeel_test_reverse(3, edges, peel_order, xors);
  xors[0] = xors[1] = xors[2] = xors[3] = xors[4] = 0;
  libfilter_unpeel_test_reverse(4, edges, peel_order, xors);
  xors[0] = xors[1] = xors[2] = xors[3] = xors[4] = 0;
  libfilter_unpeel_test_reverse(5, edges, peel_order, xors);
}

void libfilter_unpeel_test() {
  libfilter_unpeel_test_0();
  libfilter_unpeel_test_1();
  libfilter_unpeel_test_2();
}

void libfilter_peel_once_test() {
  libfilter_peel_once_test_0();
  libfilter_peel_once_test_1();
}

void libfilter_peel_test() {
  libfilter_peel_test_0();
  libfilter_peel_test_1();
}

void libfilter_full_test_1() {
  libfilter_edge e0 = {.vertex_ = {0, 1, 2}, .fingerprint_ = 1};
  libfilter_edge e1 = {.vertex_ = {1, 2, 3}, .fingerprint_ = 2};
  libfilter_edge e2 = {.vertex_ = {2, 3, 4}, .fingerprint_ = 2};
  const libfilter_edge edges[] = {e0, e1, e2};
  libfilter_edge_peel peel_order[5];
  libfilter_peel_node* peel_nodes = libfilter_init_peel_nodes(5);
  libfilter_populate_peel_nodes(3, edges, peel_nodes);
  libfilter_peel(5, edges, peel_nodes, peel_order);
  uint8_t xors[5] = {};
  libfilter_unpeel_test_reverse(3, edges, peel_order, xors);
}

void libfilter_full_test() {
  libfilter_full_test_1();
}


int main() {
  libfilter_unpeel_test();
  libfilter_peel_once_test();
  libfilter_peel_test();
  libfilter_full_test();

  // size_t n, m;
  // scanf("%ld", &n);
  // scanf("%ld", &m);
  // printf("%ld\n", libfilter_round_trip_test(n, m));
}
