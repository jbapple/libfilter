#include "filter/peel.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>

size_t libfilter_peel_test(size_t n, size_t m) {
retry:;
  uint64_t* hashes = malloc(n * sizeof(uint64_t));
  size_t to_fill = n * sizeof(uint64_t);
  while (to_fill > 0) {
    const size_t filled =
        getrandom(&((char*)hashes)[n * sizeof(uint64_t) - to_fill], to_fill, 0);
    to_fill = to_fill - filled;
    printf("filled %lu out of %lu\n", n * sizeof(uint64_t) - to_fill,
           n * sizeof(uint64_t));
    // printf("%s\n", strerror(errno));
  }

  libfilter_edge* edges = libfilter_init_edges(n, m, hashes);
  free(hashes);
  libfilter_peel_node* nodes = libfilter_init_peel_nodes(m);
  libfilter_populate_peel_nodes(n, edges, nodes);
  uint64_t* result = malloc(m * sizeof(uint64_t));
  uint8_t* xors = calloc(m, sizeof(uint8_t));
  size_t answer =  libfilter_peel(n, m, edges, nodes, result, xors);
  if (answer < m) {
    free(xors);
    free(result);
    free(nodes);
    free(edges);
    goto retry;
  }
  for (size_t i = 0; i < n; ++i) {
    printf("%ld\n", i);
    assert(libfilter_find_edge(&edges[i], xors));
  }
  free(xors);
  free(result);
  for (size_t i = 0; i < m; ++i) {
    free(nodes[i].edges_);
  }
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
