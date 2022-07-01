#include "../peel.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  libfilter_peel_node peel_nodes[5] = {};
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
