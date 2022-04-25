#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define LIBFILTER_EDGE_ARITY 3

typedef struct {
  size_t vertex_[LIBFILTER_EDGE_ARITY];
} libfilter_edge;

// Make random edges
libfilter_edge* libfilter_init_edges(size_t n, size_t m, const uint64_t* hashes) {
  libfilter_edge* result = malloc(n * sizeof(libfilter_edge));
  if (NULL == result) return result;
  for(size_t i = 0; i < n; ++i) {
    uint64_t hash = hashes[i];
    for (int j = 0; j < LIBFILTER_EDGE_ARITY; ++j) {
      const unsigned __int128 tmp = (unsigned __int128)hash * (unsigned __int128)m;
      result[i].vertex_[j] = tmp >> 64;
      assert(result[i].vertex_[j] < m);
      hash = tmp;
    }
  }
  return result;
}

typedef struct {
  size_t count_;
  size_t* edges_;
} libfilter_peel_node;

libfilter_peel_node* libfilter_init_peel_nodes(size_t m) {
  libfilter_peel_node* result = calloc(m, sizeof(libfilter_peel_node));
  return result;
}

void libfilter_populate_peel_nodes(size_t n, const libfilter_edge* edges,
                                   libfilter_peel_node* nodes) {
  for (size_t i = 0; i < n; ++i) {
    for(int j = 0; j < LIBFILTER_EDGE_ARITY; ++j) {
      libfilter_peel_node * node = &nodes[edges[i].vertex_[j]];
      node->count_++;
      node->edges_ = realloc(node->edges_, node->count_ * sizeof(size_t));
      node->edges_[node->count_ - 1] = i;
    }
  }
}

// returns number of new peelable vertexes uncovered. Always less than
// LIBFILTER_EDGE_ARITY. Adds new peelable nodes to vertex_out.
size_t libfilter_peel_once(size_t n, uint64_t vertex_number, const libfilter_edge* edges,
                           libfilter_peel_node* nodes, uint64_t* vertex_out) {
  size_t result = 0;
  assert(nodes[vertex_number].count_ == 1);
  const uint64_t edge_number = nodes[vertex_number].edges_[0];
  assert (edge_number < n);
  for (int i = 0; i < LIBFILTER_EDGE_ARITY; ++i) {
    const size_t vertex = edges[edge_number].vertex_[i];
    for (size_t j = 0; j < nodes[vertex].count_; ++j) {
      if (nodes[vertex].edges_[j] == edge_number) {
        nodes[vertex].edges_[j] = nodes[vertex].edges_[nodes[vertex].count_ - 1];
      }
    }
    //nodes[vertex].edges_ =
    //  realloc(nodes[vertex].edges_, (nodes[vertex].count_ - 1) * sizeof(size_t));
    nodes[vertex].count_--;
    if (nodes[vertex].count_ == 1 && vertex != vertex_number) {
      // New peelable nodes
      vertex_out[result++] = vertex;
    }
  }
  return result;
}

// returns number of nodes peeled
size_t libfilter_peel(size_t n, size_t m, const libfilter_edge* edges,
                      libfilter_peel_node* nodes, uint64_t* vertex_out) {
  uint64_t begin_stack = 0, end_stack = 0, to_see = 0;
  while (to_see < m) {
    if (nodes[to_see].count_ > 1) {
      ++to_see;
      continue;
    }
    vertex_out[end_stack++] = to_see++;
  }
  while (begin_stack < end_stack) {
    // invariant:
    assert(nodes[vertex_out[begin_stack]].count_ < 2);
    if (nodes[vertex_out[begin_stack]].count_ == 0) {
      ++begin_stack;
      continue;
    }
    end_stack += libfilter_peel_once(n, vertex_out[begin_stack], edges, nodes,
                                     &vertex_out[end_stack]);
    ++begin_stack;
  }
  // nodes left include everything not in vertex_out[0, begin_stack). Those nodes are part
  // of the 2-core.
  if (begin_stack + 1 == m) {
    uint64_t everything = 0;
    for (size_t i = 0; i < m; ++i) {
      everything ^= i;
    }
    for (size_t i = 0; i < begin_stack; ++i) {
      everything ^= vertex_out[i];
    }
    printf("remainder %lu\n", everything);
    printf("first %lu\n", vertex_out[0]);
    printf("last %lu\n", vertex_out[begin_stack]);
    printf("count %lu\n", nodes[everything].count_);
    for (size_t i = 0; i < nodes[everything].count_; ++i) {
      printf("edge %lu\n", nodes[everything].edges_[i]);
    }
  }
  return begin_stack;
}
