//#include <unordered_map>
//#include <vector>
//#include <cassert>
#include <assert.h>
//#include <utility>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// using namespace std;

typedef int64_t Node;
typedef int64_t Edge;
// typedef unordered_map<Edge, vector<Node>> Graph;
typedef const Edge* Graph;
const Node* edge2node(Graph g, int64_t arity, Edge e) { return g + arity * e; }

typedef struct Hood_str {
  int64_t count;
  Edge combo;
} Hood;

typedef Hood* Phrag;

void phrag(Graph g, int64_t arity, int64_t edgeCount, Phrag p) {
  for (Edge e = 0; e < edgeCount; ++e) {
    const Node* hood = edge2node(g, arity, e);
    assert(p[hood[0]].count >= 0);
    // assert(p[hood[0]].combo == 0);
    for (int64_t i = 0; i < arity; ++i) {
      p[hood[i]].count++;
      p[hood[i]].combo ^= e;
    }
  }
}

typedef struct Inc_str {
  Edge edge;
  int64_t next;
} Inc;

typedef Inc* Flipped;

void makeInc(Graph g, int64_t arity, int64_t edgeCount, int64_t nodeCount,
             int64_t chains[nodeCount], Inc f[arity * edgeCount]) {
  for(int64_t i = 0; i < nodeCount; ++i) {
    chains[i] = -1;
  }
  for (Edge e = 0; e < edgeCount; ++e) {
    const Node* hood = edge2node(g, arity, e);
    for (int64_t i = 0; i < arity; ++i) {
      int64_t next_idx = e * arity + i;
      Inc* slot = NULL;
      assert(hood[i] >= 0);
      if (-1 == chains[hood[i]]) {
        chains[hood[i]] = next_idx;
        slot = &f[next_idx];
      } else {
        assert(chains[hood[i]] >= 0);
        assert(chains[hood[i]] < arity * edgeCount);
        slot = &f[chains[hood[i]]];
        while (slot->next != -1) {
          Inc * next_slot = &f[slot->next];
          assert(next_slot != NULL);
          slot = next_slot;
        }
        slot->next = next_idx;
        slot = &f[slot->next];
      }
      slot->edge = e;
      slot->next = -1;
    }
  }
}

typedef struct PeelStep_str {
  Node node;
  Edge edge;
} PeelStep;

bool peel(Phrag p, Graph g, int64_t arity, int64_t nodeCount, int64_t edgeCount,
          PeelStep* trace) {
  int64_t qbegin = 0, qend = 0;
  for (int64_t i = 0; i < nodeCount; ++i) {
    if (p[i].count < 2) {
      trace[qend].node = i;
      trace[qend].edge = p[i].combo;
      ++qend;
    }
  }
  while (qend > qbegin) {
    assert(qend <= nodeCount);
    Node n = trace[qbegin].node;
    ++qbegin;
    assert(p[n].count < 2);
    assert(p[n].count >= 0);
    if (p[n].count == 0) {
      trace[qbegin - 1].edge = -1;
      continue;
    }
    assert(p[n].count == 1);
    assert(p[n].combo >= 0);
    assert(p[n].combo < edgeCount);
    Edge e = p[n].combo;
    const Node* buds = edge2node(g, arity, p[n].combo);
    for (int64_t j = 0; j < arity; ++j) {
      Node v = buds[j];
      assert(v >= 0);
      assert(v < nodeCount);
      // if (v == n) continue;
      assert(p[v].count >= 0);
      // if (p[v].count == 0) continue;
      assert(p[v].count != 0);
      p[v].count -= 1;
      p[v].combo ^= e;
      if (p[v].count == 1) {
        trace[qend].node = v;
        trace[qend].edge = p[v].combo;
        ++qend;
      }
    }
  }
  assert(qend == qbegin);
  assert(qend <= nodeCount);
  return qend == nodeCount;
}

int64_t count(Node n, int64_t* chains /*[nodeCount]*/,
              Flipped f /*[arity * edgeCount]*/) {
  if (-1 == chains[n]) return 0;
  int64_t result = 1;
  for (Inc* i = &f[chains[n]]; i->next != -1; i = &f[i->next]) {
    ++result;
  }
  return result;
}

Edge popback(Node n, int64_t* chains /*[nodeCount]*/, Flipped f /*[arity * edgeCount]*/) {
  assert(-1 != chains[n]);
  Inc* before = &f[chains[n]];
  if (-1 == before->next) {
    chains[n] = -1;
    return before->edge;
  }
  Inc* here = &f[before->next];
  while (here->next != -1) {
    before = here;
    here = &f[here->next];
  }
  before->next = -1;
  return here->edge;
}

Edge back(Node n, const int64_t* chains /*[nodeCount]*/,
          const Inc* f /*[arity * edgeCount]*/) {
  assert(-1 != chains[n]);
  const Inc* before = &f[chains[n]];
  while (before->next != -1) {
    before = &f[before->next];
  }
  return before->edge;
}

void pop(Node n, Edge e, int64_t* chains /*[nodeCount]*/,
         Flipped f /*[arity * edgeCount]*/) {
  assert(-1 != chains[n]);
  Inc* before = &f[chains[n]];
  if (e == before->edge) {
    chains[n] = before->next;
    return;
  }
  assert(before->next != -1);
  Inc* here = &f[before->next];
  while (here->edge != e) {
    assert(here->next != -1);
    before = here;
    here = &f[here->next];
  }
  before->next = here->next;
}

void removeEdgeWithStash(Edge e, int64_t* chains /*[nodeCount]*/,
                         Flipped f /*[arity * edgeCount]*/, Graph g, int64_t arity,
                         int64_t nodeCount, PeelStep* trace, int64_t* qend) {
  const Node* buds = edge2node(g, arity, e);
  for (int64_t j = 0; j < arity; ++j) {
    Node v = buds[j];
    assert(v >= 0);
    assert(v < nodeCount);
    // if (v == n) continue;
    assert(count(v, chains, f) >= 0);
    // if (p[v].count == 0) continue;
    assert(count(v, chains, f) != 0);
    pop(v, e, chains, f);
    if (count(v, chains, f) == 1) {
      trace[*qend].node = v;
      trace[*qend].edge = f[chains[v]].edge;
      *qend += 1;
    }
  }
}

int64_t peelWithStash(int64_t* chains /*[nodeCount]*/, Flipped f /*[arity * edgeCount]*/,
                      Graph g, int64_t arity, int64_t nodeCount, int64_t edgeCount,
                      PeelStep* trace, Edge stash[edgeCount]) {
  int64_t result = 0;
  int64_t qbegin = 0, qend = 0;
  for (int64_t i = 0; i < nodeCount; ++i) {
    if (count(i, chains, f) < 2) {
      trace[qend].node = i;
      trace[qend].edge = (chains[i] == -1) ? -1 : f[chains[i]].edge;
      ++qend;
    }
  }
peel:
  while (qend > qbegin) {
    assert(qend <= nodeCount);
    Node n = trace[qbegin].node;
    ++qbegin;
    assert(count(n, chains, f) < 2);
    assert(count(n, chains, f) >= 0);
    if (count(n, chains, f) == 0) {
      trace[qbegin - 1].edge = -1;
      continue;
    }
    assert(count(n, chains, f) == 1);
    assert(f[chains[n]].edge >= 0);
    assert(f[chains[n]].edge < edgeCount);
    removeEdgeWithStash(f[chains[n]].edge, chains, f, g, arity, nodeCount, trace, &qend);
  }
  if (qend < nodeCount) {
    int64_t min_inc = edgeCount + 1;
    Node n = -1;
    for (Node i = 0; i < nodeCount; ++i) {
      int64_t c = count(i, chains, f);
      if (c > 1 && c < min_inc) {
        min_inc = c;
        n = i;
      }
    }
    assert(n >= 0);
    assert(n < nodeCount);
    assert(min_inc <= edgeCount);
    assert(min_inc > 1);
    Edge e = back(n, chains, f);
    stash[result] = e;
    ++result;
    removeEdgeWithStash(e, chains, f, g, arity, nodeCount, trace, &qend);
    goto peel;
  }
  assert(qend == qbegin);
  assert(qend == nodeCount);
  return result;
}

void unwind(Graph g, int64_t nodeCount, int64_t edgeCount, int64_t arity,
            const PeelStep* p, const unsigned char edgeSigs[], unsigned char result[]) {
  for (int64_t i = nodeCount - 1; i >= 0; i -= 1) {
    if (p[i].edge == -1) continue;
    assert(p[i].edge >= 0);
    assert(p[i].edge < edgeCount);
    assert(p[i].node >= 0);
    assert(p[i].node < nodeCount);
    result[p[i].node] = edgeSigs[p[i].edge];
    const Node* hood = edge2node(g, arity, p[i].edge);
    for (int64_t j = 0; j < arity; ++j) {
      if (hood[j] == p[i].node) continue;
      result[p[i].node] ^= result[hood[j]];
    }
    unsigned char test = 0;
    for (int64_t j = 0; j < arity; ++j) {
      test ^= result[hood[j]];
    }
    assert(test == edgeSigs[p[i].edge]);
  }
}

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)

typedef struct {
  uint64_t state;
  uint64_t inc;
} pcg32_random_t;

uint32_t pcg32_random_r(pcg32_random_t* rng) {
  uint64_t oldstate = rng->state;
  // Advance internal state
  rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
  // Calculate output function (XSH RR), uses old state for max ILP
  uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
  uint32_t rot = oldstate >> 59u;
  return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

uint64_t pcg64(pcg32_random_t* rng) {
  uint64_t result = pcg32_random_r(rng);
  result = (result << 32) | pcg32_random_r(rng);
  return result;
}

void makeGraph(int64_t arity, int64_t edgeCount, int64_t nodeCount, Node* g,
               pcg32_random_t* rnd) {
  int64_t arenaCount = nodeCount / arity;
  for (Edge e = 0; e < edgeCount; ++e) {
    Node* n = &g[arity * e];
    for (int64_t j = 0; j < arity; ++j) {
      n[j] = ((pcg32_random_r(rnd) * arenaCount) >> 32) + j * arenaCount;
      assert(n[j] < nodeCount);
      if (j > 0) {
        assert(n[j] > n[j - 1]);
      }
    }
  }
}

bool testPeelable(int64_t arity, int64_t edgeCount, int64_t nodeCount,
                  pcg32_random_t* rnd, Node* g, Hood* h, PeelStep* p) {
  memset(g, 0, sizeof(Node) * arity * edgeCount);
  memset(h, 0, sizeof(Hood) * nodeCount);
  memset(p, 0, sizeof(PeelStep) * nodeCount);
  makeGraph(arity, edgeCount, nodeCount, g, rnd);
  phrag(g, arity, edgeCount, h);
  return peel(h, g, arity, nodeCount, edgeCount, p);
}

int64_t testPeelWithStash(int64_t arity, int64_t edgeCount, int64_t nodeCount,
                       pcg32_random_t* rnd, Node g[arity * edgeCount],
                       Inc f[arity * edgeCount], int64_t chains[nodeCount],
                       PeelStep p[nodeCount], Edge stash[edgeCount]) {
  memset(g, 0, sizeof(Node) * arity * edgeCount);
  memset(f, 0, sizeof(Inc) * arity * edgeCount);
  memset(chains, 0, sizeof(int64_t) * nodeCount);
  memset(p, 0, sizeof(PeelStep) * nodeCount);
  memset(stash, 0, sizeof(Edge) * edgeCount);
  makeGraph(arity, edgeCount, nodeCount, g, rnd);
  makeInc(g, arity, edgeCount, nodeCount, chains, f);
  return peelWithStash(chains, f, g, arity, nodeCount, edgeCount, p, stash);
}

double testMany(int64_t trials, int64_t arity, int64_t edgeCount, int64_t nodeCount,
                pcg32_random_t* rnd) {
  Node* g = malloc(sizeof(Node) * arity * edgeCount);
  Hood* h = malloc(sizeof(Hood) * nodeCount);
  PeelStep* p = malloc(sizeof(PeelStep) * nodeCount);
  int64_t result = 0;
  for (int64_t j = 0; j < trials; ++j) {
    result += testPeelable(arity, edgeCount, nodeCount, rnd, g, h, p);
  }
  free(p);
  free(h);
  free(g);
  return ((double)result) / ((double)trials);
}

double testManyWithStash(int64_t trials, int64_t arity, int64_t edgeCount, int64_t nodeCount,
                pcg32_random_t* rnd) {
  Node* g = malloc(sizeof(Node) * arity * edgeCount);
  Inc* f = malloc(sizeof(Inc) * arity * edgeCount);
  int64_t* chains = malloc(sizeof(int64_t) * nodeCount);
  PeelStep* p = malloc(sizeof(PeelStep) * nodeCount);
  Edge* stash = malloc(sizeof(Edge) * edgeCount);
  int64_t result = 0;
  for (int64_t j = 0; j < trials; ++j) {
    result += testPeelWithStash(arity, edgeCount, nodeCount, rnd, g, f, chains, p, stash);
  }
  free(stash);
  free(p);
  free(chains);
  free(f);
  free(g);
  return ((double)result) / ((double)trials);
}

void testPeelableSpec() {
  pcg32_random_t rnd = {1, 1};
  printf("%f\n", testMany(100, 3, 800, 1024, &rnd));
}



int64_t testXorFilter(int64_t arity, int64_t edgeCount, int64_t nodeCount,
                      pcg32_random_t* rnd, Node* g, Hood* h, PeelStep* p,
                      unsigned char* edgeSigs, unsigned char* result) {
  memset(g, 0, sizeof(Node) * arity * edgeCount);
  memset(h, 0, sizeof(Hood) * nodeCount);
  memset(p, 0, sizeof(PeelStep) * nodeCount);
  makeGraph(arity, edgeCount, nodeCount, g, rnd);
  phrag(g, arity, edgeCount, h);
  if (!peel(h, g, arity, nodeCount, edgeCount, p)) return -1;
  for (int64_t j = 0; j < edgeCount; ++j) {
    edgeSigs[j] = pcg32_random_r(rnd);
  }
  unwind(g, nodeCount, edgeCount, arity, p, edgeSigs, result);
  int64_t uhohs = 0;
  for (int64_t j = 0; j < edgeCount; ++j) {
    unsigned char actual = 0;
    for (int64_t k = 0; k < arity; ++k) {
      Node n = edge2node(g, arity, j)[k];
      actual ^= result[n];
    }
    uhohs += (actual != edgeSigs[j]);
  }
  return uhohs;
}

double testManyXor(int64_t trials, int64_t arity, int64_t edgeCount, int64_t nodeCount,
                   pcg32_random_t* rnd) {
  Node* g = malloc(sizeof(Node) * arity * edgeCount);
  Hood* h = malloc(sizeof(Hood) * nodeCount);
  PeelStep* p = malloc(sizeof(PeelStep) * nodeCount);
  unsigned char* edgeSigs = malloc(edgeCount);
  unsigned char* result = malloc(nodeCount);
  int64_t bad = 0;
  for (int64_t j = 0; j < trials; ++j) {
    int64_t sad =
        testXorFilter(arity, edgeCount, nodeCount, rnd, g, h, p, edgeSigs, result);
    if (-1 == sad) {
      j -= 1;
    } else {
      bad += sad;
    }
  }
  free(result);
  free(edgeSigs);
  free(p);
  free(h);
  free(g);
  return ((double)bad) / ((double)trials * edgeCount);
}

void testXor() {
  pcg32_random_t rnd = {1, 1};
  printf("%f\n", testManyXor(100, 3, 800, 1024, &rnd));
}

void makeFuse(int64_t edgeCount, Node* g, int64_t arity, int64_t nodeCount,
              int64_t segmentCount, const uint64_t* edgeHashes) {
  // nodeCount = segLength * (segmentCount + arity - 1)
  int64_t segmentLength = nodeCount / (segmentCount + arity - 1);
  for (int64_t j = 0; j < edgeCount; ++j) {
    pcg32_random_t rnd = {edgeHashes[j], 1};
    int32_t seg = (pcg32_random_r(&rnd) * segmentCount) >> 32;
    for (int64_t k = 0; k < arity; ++k) {
      int64_t offset = (pcg32_random_r(&rnd) * segmentLength) >> 32;
      assert(offset >= 0);
      Node n = (seg + k) * segmentLength + offset;
      assert(n >= 0);
      assert(n < nodeCount);
      g[j * arity + k] = n;
    }
  }
}

int64_t testFusePeel(int64_t edgeCount, int64_t arity, int64_t nodeCount,
                     int64_t segmentCount, int64_t samples) {
  int64_t goods = 0;
  Node* g = malloc(sizeof(Node) * arity * edgeCount);
  uint64_t* edgeHashes = malloc(sizeof(uint64_t) * edgeCount);
  Hood* h = malloc(sizeof(Hood) * nodeCount);
  PeelStep* p = malloc(sizeof(PeelStep) * nodeCount);
  for (int64_t j = 0; j < samples; ++j) {
    pcg32_random_t rnd = {j, 1};
    for (int64_t k = 0; k < edgeCount; ++k) {
      edgeHashes[k] = pcg64(&rnd);
    }
    makeFuse(edgeCount, g, arity, nodeCount, segmentCount, edgeHashes);
    memset(h, 0, sizeof(Hood) * nodeCount);
    memset(p, 0, sizeof(PeelStep) * nodeCount);
    phrag(g, arity, edgeCount, h);
    goods += peel(h, g, arity, nodeCount, edgeCount, p);
  }
  free(p);
  free(h);
  free(edgeHashes);
  free(g);
  return goods;
}

double testFusePeelWithStash(int64_t edgeCount, int64_t arity, int64_t nodeCount,
                              int64_t segmentCount, int64_t samples) {
  int64_t stash_count = 0;
  Node* g = malloc(sizeof(Node) * arity * edgeCount);
  uint64_t* edgeHashes = malloc(sizeof(uint64_t) * edgeCount);

  PeelStep* p = malloc(sizeof(PeelStep) * nodeCount);

  Inc* f = malloc(sizeof(Inc) * arity * edgeCount);
  int64_t* chains = malloc(sizeof(int64_t) * nodeCount);
  Edge* stash = malloc(sizeof(Edge) * edgeCount);

  for (int64_t j = 0; j < samples; ++j) {
    pcg32_random_t rnd = {j, 1};
    for (int64_t k = 0; k < edgeCount; ++k) {
      edgeHashes[k] = pcg64(&rnd);
    }
    makeFuse(edgeCount, g, arity, nodeCount, segmentCount, edgeHashes);

    memset(f, 0, sizeof(Inc) * arity * edgeCount);
    memset(chains, 0, sizeof(int64_t) * nodeCount);

    memset(stash, 0, sizeof(Edge) * edgeCount);

    memset(p, 0, sizeof(PeelStep) * nodeCount);
    makeInc(g, arity, edgeCount, nodeCount, chains, f);

    stash_count += peelWithStash(chains, f, g, arity, nodeCount, edgeCount, p, stash);
  }
  free(stash);
  free(chains);
  free(f);
  free(p);

  free(edgeHashes);
  free(g);
  return ((double)stash_count) / ((double)samples);
}

int paperTest() {

  pcg32_random_t rnd = {1, 1};
  printf("%f\n", testManyWithStash(100, 3, 8000, 10000, &rnd));

  for (int64_t i = 3; i < 8; ++i) {
    for (int64_t j = 4; j < 1024; j *= 2) {
      double result = testFusePeelWithStash(8000, i, 10000, j, 10);
      printf("%ld %ld %ld\n", (int64_t)result, i, j);
    }
  }
  return 0;
  printf("%ld %ld %lf\n", 5L, 100L,
         testFusePeelWithStash(910 * 100, 5, 1000 * 100, 200, 1));
  return 0;
  printf("%ld %ld %ld\n", 5L, 200L, testFusePeel(950 * 100, 5, 1000 * 100, 200, 100));
  printf("%ld %ld %ld\n", 5L, 200L, testFusePeel(950 * 1000, 5, 1000 * 1000, 200, 100));
  return 0;
  printf("%ld %ld %ld\n", 3L, 100L, testFusePeel(879 * 10000, 3, 1000 * 10000, 100, 100));
  printf("%ld %ld %ld\n", 4L, 200L, testFusePeel(943 * 10000, 4, 1000 * 10000, 200, 100));
  printf("%ld %ld %ld\n", 7L, 500L, testFusePeel(973 * 10000, 7, 1000 * 10000, 500, 100));
  return 0;
  for (int64_t i = 3; i < 8; ++i) {
    for (int64_t j = 4; j < 1024; j *= 2) {
      int64_t result = testFusePeel(900000, i, 1000000, j, 100);
      if (result) printf("%ld %ld %ld\n", i, j, result);
    }
  }
}

int main() {
  paperTest();
}
