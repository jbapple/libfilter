#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define LIBFILTER_EDGE_ARITY 3

typedef struct {
  size_t vertex_[LIBFILTER_EDGE_ARITY];
  uint8_t fingerprint_;
} libfilter_edge;


// void libfilter_make_edge(uint64_t hash, size_t m, libfilter_edge* result) {
//     for (int j = 0; j < LIBFILTER_EDGE_ARITY; ++j) {
//       const unsigned __int128 tmp = (unsigned __int128)hash * (unsigned __int128)(m - j);
//       result->vertex_[j] = tmp >> 64;
//       for (int i = 0; i < j; ++i) {
//         if (result->vertex_[j] >= result->vertex_[i]) ++result->vertex_[j];
//       }
//       assert(result->vertex_[j] < m);
//       // insertion sort
//       for (int i = 0; i < j; ++i) {
//         if (result->vertex_[j] < result->vertex_[i]) {
//           size_t 
//           for (int k = i; k < k
//           break;
//         }
//       }
//       hash = tmp;
//     }
//     result->fingerprint_ = hash >> 56;


// }

// Make random edges
// TODO: what if two nodes coincide
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
    result[i].fingerprint_ = hash >> 56;
  }
  return result;
}

bool libfilter_find_edge(const libfilter_edge* edge, const uint8_t* xors) {
  uint8_t fingerprint = edge->fingerprint_;
  for (int i = 0; i < LIBFILTER_EDGE_ARITY; ++i) {
    fingerprint ^= xors[edge->vertex_[i]];
  }
  return 0 == fingerprint;
}

typedef struct {
  size_t count_;
  uint64_t edges_;
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
      node->edges_ = node->edges_ ^ i;
    }
  }
}

typedef struct {
  size_t edge_number_;
  size_t peeled_vertex_;
} libfilter_edge_peel;

// // returns number of new peelable vertexes uncovered. Always less than
// // LIBFILTER_EDGE_ARITY. Adds new peelable nodes to vertex_out.
// size_t libfilter_peel_once(size_t n, uint64_t vertex_number, const libfilter_edge* edges,
//                            libfilter_peel_node* nodes, libfilter_edge_peel * to_peel) {
//   size_t result = 0;
//   assert(nodes[vertex_number].count_ == 1);
//   const uint64_t edge_number = nodes[vertex_number].edges_[0];
//   assert(edge_number < n);
//   for (int i = 0; i < LIBFILTER_EDGE_ARITY; ++i) {
//     const size_t vertex = edges[edge_number].vertex_[i];
//     for (size_t j = 0; j < nodes[vertex].count_; ++j) {
//       if (nodes[vertex].edges_[j] == edge_number) {
//         nodes[vertex].edges_[j] = nodes[vertex].edges_[nodes[vertex].count_ - 1];
//       }
//     }
//     // nodes[vertex].edges_ =
//     //  realloc(nodes[vertex].edges_, (nodes[vertex].count_ - 1) * sizeof(size_t));
//     nodes[vertex].count_--;
//     if (nodes[vertex].count_ == 1 && vertex != vertex_number) {
//       // New peelable nodes
//       to_peel[result].edge_number_ = nodes[vertex].edges_[0];
//       to_peel[result].peeled_vertex_ = vertex;
//       ++result;
//     }
//   }
//   return result;
// }


// // returns number of nodes peeled
// size_t libfilter_peel(size_t n, size_t m, const libfilter_edge* edges,
//                       libfilter_peel_node* nodes, uint64_t* vertex_out,
//                       uint64_t* edge_out) {
//   uint64_t begin_stack = 0, end_stack = 0, to_see = 0;
//   while (to_see < m) {
//     if (nodes[to_see].count_ > 1) {
//       ++to_see;
//       continue;
//     }
//     vertex_out[end_stack++] = to_see++;
//   }
//   while (begin_stack < end_stack) {
//     // invariant:
//     assert(nodes[vertex_out[begin_stack]].count_ < 2);
//     if (nodes[vertex_out[begin_stack]].count_ == 0) {
//       ++begin_stack;
//       continue;
//     }
//     *edge_out++ = nodes[vertex_out[begin_stack]].edges_[0];
//     end_stack += libfilter_peel_once(n, vertex_out[begin_stack], edges, nodes,
//                                      &vertex_out[end_stack]);
//     ++begin_stack;
//   }
//   return begin_stack;
// }

// void libfilter_unpeel(size_t m, const libfilter_edge* edges,
//                       const uint64_t* vertex_out, const uint64_t* edge_out,
//                       uint8_t* xors) {
//   xors[edges[edge_out[m - 1]].vertex_[0]] = edges[edge_out[m - 1]].fingerprint_;
//   for(size_t i = m-1;i != 0; --i) {
//     size_t j = i-1;
//     uint8_t xor_remainder = edges[edge_out[j]].fingerprint_;
//     for (int k = 0; k < LIBFILTER_EDGE_ARITY; ++k) {
//       xor_remainder ^= xors[edges[edge_out[j]].vertex_[k]];
//     }
//     //xors[
//   }
// }
