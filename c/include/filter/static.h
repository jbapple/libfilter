#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "memory.h"

typedef struct {
  size_t length_;
  libfilter_region region_;
} libfilter_static;

libfilter_static libfilter_static_construct(size_t n, const uint64_t* hashes);

#define LIBFILTER_EDGE_ARITY 3

typedef struct {
  size_t vertex_[LIBFILTER_EDGE_ARITY];
  uint8_t fingerprint_;
} libfilter_edge;

// return if v is among the first seen vertexes in vertex
bool libfilter_in_edge(size_t v, int seen, const size_t vertex[LIBFILTER_EDGE_ARITY]) {
  for (int i = 0; i < seen; ++i) {
    if (v == vertex[i]) return true;
  }
  return false;
}

// makes a hyperedge from hash where each vertex is less than m
void libfilter_make_edge(uint64_t hash, size_t m, libfilter_edge* result) {
  for (int j = 0; j < LIBFILTER_EDGE_ARITY; ++j) {
    const unsigned __int128 tmp = (unsigned __int128)hash * (unsigned __int128)m;
    result->vertex_[j] = tmp >> 64;
    while (libfilter_in_edge(result->vertex_[j], j, result->vertex_)) {
      ++result->vertex_[j];
      result->vertex_[j] += (result->vertex_[j] == m) ? (-m) : 0;
    }
    assert(result->vertex_[j] < m);
    hash = hash * m;
    //printf("%ld, ", result->vertex_[j]);
  }
  result->fingerprint_ = hash >> (64 - 8);
  //printf("\n");
}

// return true if the fingerprint of edge matches the expected fingerprint in xors
bool libfilter_find_edge(const libfilter_edge* edge, const uint8_t* xors) {
  uint8_t fingerprint = edge->fingerprint_;
  for (int i = 0; i < LIBFILTER_EDGE_ARITY; ++i) {
    fingerprint ^= xors[edge->vertex_[i]];
  }
  return 0 == fingerprint;
}

bool libfilter_static_lookup(const libfilter_static* filter, uint64_t hash) {
  libfilter_edge e;
  libfilter_make_edge(hash, filter->length_, &e);
  return libfilter_find_edge(&e, (const uint8_t*)filter->region_.block);
}
