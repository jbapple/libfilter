#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "filter/static.h"

#include "memory-internal.h"

// Make n random edges from hashes with nodes less than m
void libfilter_init_edges(size_t n, size_t m, const uint64_t* hashes /* [n] */,
                          libfilter_edge* result /* [n] */) {
  if (NULL == result) return;
  for (size_t i = 0; i < n; ++i) libfilter_make_edge(hashes[i], m, &result[i]);
}

// structure temporarily used for peeling a hypergraph
typedef struct {
  size_t count_;   // number of hyperedges incident to this node
  uint64_t edges_; // xor of the remaining hyperedges incident to this node
} libfilter_peel_node;

// // Allocate and zero-initialize libfilter_peel_node array of size m
// libfilter_peel_node* libfilter_init_peel_nodes(size_t m) {
//   libfilter_peel_node* result = calloc(m, sizeof(libfilter_peel_node));
//   return result;
// }

// take n edges and initialize nodes
void libfilter_populate_peel_nodes(size_t num_edges, const libfilter_edge* edges,
                                   libfilter_peel_node* nodes) {
  for (size_t i = 0; i < num_edges; ++i) {
    for(int j = 0; j < LIBFILTER_EDGE_ARITY; ++j) {
      libfilter_peel_node * node = &nodes[edges[i].vertex_[j]];
      node->count_++;
      node->edges_ = node->edges_ ^ i;
    }
  }
}

// strcture temporarily used for tracking the order of peeling
typedef struct {
  size_t edge_number_;
  size_t peeled_vertex_; // the vertex this edge was peeled at
} libfilter_edge_peel;

// peel vetex_number, given edges and nodes.
// returns number of new peelable vertexes uncovered. Always strictly less than
// LIBFILTER_EDGE_ARITY. Adds new peelable nodes to to_peel.
size_t libfilter_peel_once(uint64_t vertex_number, const libfilter_edge* edges,
                           libfilter_peel_node* nodes, libfilter_edge_peel* to_peel) {
  size_t result = 0;
  assert(nodes[vertex_number].count_ == 1);
  const uint64_t edge_number = nodes[vertex_number].edges_;
  for (int i = 0; i < LIBFILTER_EDGE_ARITY; ++i) {
    const size_t vertex = edges[edge_number].vertex_[i];
    nodes[vertex].edges_ ^= edge_number;
    nodes[vertex].count_--;
    if (nodes[vertex].count_ == 1 && vertex != vertex_number) {
      // New peelable nodes
      to_peel[result].edge_number_ = nodes[vertex].edges_;
      to_peel[result].peeled_vertex_ = vertex;
      ++result;
    }
  }
  return result;
}

// returns number of nodes peeled
// TODO: multi-threading and SIMD
size_t libfilter_peel(size_t num_nodes, const libfilter_edge* edges,
                      libfilter_peel_node* nodes, libfilter_edge_peel* to_peel) {
  uint64_t begin_stack = 0, end_stack = 0, to_see = 0;
  while (to_see < num_nodes) {
    if (nodes[to_see].count_ > 1) {
      ++to_see;
      continue;
    }
    to_peel[end_stack].edge_number_ = nodes[to_see].edges_;
    to_peel[end_stack].peeled_vertex_ = to_see;
    ++end_stack;
    ++to_see;
  }
  while (begin_stack < end_stack) {
    // invariant:
    assert(nodes[to_peel[begin_stack].peeled_vertex_].count_ < 2);
    if (nodes[to_peel[begin_stack].peeled_vertex_].count_ == 0) {
      ++begin_stack;
      continue;
    }
    //*edge_out++ = nodes[to_peel[begin_stack]].edges_;
    end_stack += libfilter_peel_once(to_peel[begin_stack].peeled_vertex_, edges, nodes,
                                     &to_peel[end_stack]);
    ++begin_stack;
  }
  if (begin_stack != num_nodes) {
    for (size_t i = 0; i < num_nodes; ++i) {
      assert(nodes[i].count_ != 1);
    }
  }
  return begin_stack;
}

void libfilter_unpeel(size_t node_count, const libfilter_edge* edges,
                      const libfilter_edge_peel* peel_order, uint8_t* xors) {
  for (size_t i = 0; i < node_count; ++i) {
    size_t j = node_count - 1 - i;
    uint8_t xor_remainder = edges[peel_order[j].edge_number_].fingerprint_;
    assert(0 == xors[peel_order[j].peeled_vertex_]);
    for (int k = 0; k < LIBFILTER_EDGE_ARITY; ++k) {
      xor_remainder ^= xors[edges[peel_order[j].edge_number_].vertex_[k]];
    }
    xors[peel_order[j].peeled_vertex_] = xor_remainder;
  }
}
