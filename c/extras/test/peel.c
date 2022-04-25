#include <stdbool.h>
#include <stdio.h>
#include <sys/random.h>

#include "filter/peel.h"

size_t libfilter_peel_test(size_t n, size_t m) {
  uint64_t* hashes = malloc(n * sizeof(uint64_t));
  getrandom(hashes, n * sizeof(uint64_t), 0);
  libfilter_edge* edges = libfilter_init_edges(n, m, hashes);
  free(hashes);
  libfilter_peel_node* nodes = libfilter_init_peel_nodes(m);
  libfilter_populate_peel_nodes(n, edges, nodes);
  uint64_t* result = malloc(m * sizeof(uint64_t));
  const size_t answer = libfilter_peel(n, m, edges, nodes, result);
  free(result);
  free(nodes);
  free(edges);
  return answer;
}

int main() {
  size_t n, m;
  scanf("%ld", &n);
  scanf("%ld", &m);
  printf("%ld\n", libfilter_peel_test(n, m));
}
