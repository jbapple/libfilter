package com.github.jbapple.libfilter;

public class XorFilter implements StaticFilter {
  private static int LIBFILTER_EDGE_ARITY = 3;
  private byte[] fingerprints;

  static private class Edge {
    public int[/* LIBFILTER_EDGE_ARITY */] vertex;
    public byte fingerprint;
    public Edge() { vertex = new int[LIBFILTER_EDGE_ARITY]; }
  }

  // return if v is among the first seen vertexes in vertex
  private static boolean InEdge(int v, int seen, int[/* LIBFILTER_EDGE_ARITY */] vertex) {
    for (int i = 0; i < seen; ++i) {
      if (v == vertex[i]) return true;
    }
    return false;
  }

  // makes a hyperedge from hash where each vertex is less than m
  private Edge MakeEdge(long hash) {
    Edge result = new Edge();
    for (int j = 0; j < LIBFILTER_EDGE_ARITY; ++j) {
      result.vertex[j] =
          (int) (hash % fingerprints.length); // TODO: use Java 9's multiplyHigh to do
                                              // fast range reduction with 128 bits
      while (InEdge(result.vertex[j], j, result.vertex)) {
        ++result.vertex[j];
        result.vertex[j] +=
            (result.vertex[j] == fingerprints.length) ? (-fingerprints.length) : 0;
      }
      assert (result.vertex[j] < fingerprints.length);
      hash = hash / fingerprints.length;
    }
    result.fingerprint = (byte) (hash >>> (64 - 8));
    return result;
  }

  // return true if the fingerprint of edge matches the expected fingerprint in xors
  private boolean FindEdge(Edge edge) {
    byte fingerprint = edge.fingerprint;
    for (int i = 0; i < LIBFILTER_EDGE_ARITY; ++i) {
      fingerprint ^= fingerprints[edge.vertex[i]];
    }
    return 0 == fingerprint;
  }

  public boolean FindHash64(long hash) {
    Edge e = MakeEdge(hash);
    return FindEdge(e);
  }

  static private class PeelNode {
    long count; // number of hyperedges incident to this node
    long edges; // xor of the remaining hyperedges incident to this node
  };

  // take n edges and initialize nodes
  static private void PopulatePeelNodes(Edge[] edges, PeelNode[] nodes) {
    for (int i = 0; i < edges.length; ++i) {
      for (int j = 0; j < LIBFILTER_EDGE_ARITY; ++j) {
        nodes[edges[i].vertex[j]].count++;
        nodes[edges[i].vertex[j]].edges ^= i;
      }
    }
  }

  static private class EdgePeel {
    long edge_number;
    int peeled_vertex; // the vertex this edge was peeled at
  }

  // peel vetex_number, given edges and nodes.
  // returns number of new peelable vertexes uncovered. Always strictly less than
  // LIBFILTER_EDGE_ARITY. Adds new peelable nodes to to_peel.
  private static int PeelOnce(int vertex_number, Edge[] edges, PeelNode[] nodes,
      EdgePeel[] to_peel, int edge_peel_start) {
    int result = 0;
    assert (nodes[vertex_number].count == 1);
    long edge_number = nodes[vertex_number].edges;
    for (int i = 0; i < LIBFILTER_EDGE_ARITY; ++i) {
      int vertex = edges[(int)edge_number].vertex[i];
      nodes[vertex].edges ^= edge_number;
      nodes[vertex].count--;
      if (nodes[vertex].count == 1 && vertex != vertex_number) {
        // New peelable nodes
        to_peel[edge_peel_start + result].edge_number = nodes[vertex].edges;
        to_peel[edge_peel_start + result].peeled_vertex = vertex;
        ++result;
      }
    }
    return result;
  }

  // returns number of nodes peeled
  // TODO: multi-threading and SIMD
  private static int Peel(Edge[] edges, PeelNode[] nodes, EdgePeel[] to_peel) {
    int begin_stack = 0, end_stack = 0, to_see = 0;
    while (to_see < nodes.length) {
      if (nodes[to_see].count > 1) {
        ++to_see;
        continue;
      }
      to_peel[end_stack].edge_number = nodes[to_see].edges;
      to_peel[end_stack].peeled_vertex = to_see;
      ++end_stack;
      ++to_see;
    }
    while (begin_stack < end_stack) {
      // invariant:
      assert (nodes[to_peel[begin_stack].peeled_vertex].count < 2);
      if (nodes[to_peel[begin_stack].peeled_vertex].count == 0) {
        ++begin_stack;
        continue;
      }
      //*edge_out++ = nodes[to_peel[begin_stack]].edges_;
      end_stack +=
          PeelOnce(to_peel[begin_stack].peeled_vertex, edges, nodes, to_peel, end_stack);
      ++begin_stack;
    }
    if (begin_stack != nodes.length) {
      for (int i = 0; i < nodes.length; ++i) {
        assert (nodes[i].count != 1);
      }
    }
    return begin_stack;
  }

  private void Unpeel(Edge[] edges, EdgePeel[] peel_order) {
    for (int i = 0; i < peel_order.length; ++i) {
      int j = peel_order.length - 1 - i;
      byte xor_remainder = edges[(int)peel_order[j].edge_number].fingerprint;
      assert (0 == fingerprints[peel_order[j].peeled_vertex]);
      for (int k = 0; k < LIBFILTER_EDGE_ARITY; ++k) {
        xor_remainder ^= fingerprints[edges[(int)peel_order[j].edge_number].vertex[k]];
      }
      fingerprints[peel_order[j].peeled_vertex] = xor_remainder;
    }
  }

  public XorFilter(long[] hashes) {
    int size = (int)(1.23 * hashes.length);
    while (true) {
      // initialize the fingerprints
      fingerprints = new byte[size];

      // initialize the edges
      Edge[] edges = new Edge[hashes.length];
      for (int i = 0; i < edges.length; ++i) {
        edges[i] = MakeEdge(hashes[i]);
      }

      // initialize the nodes
      PeelNode[] nodes = new PeelNode[size];
      PopulatePeelNodes(edges, nodes);

      // initialize the peel results
      EdgePeel[] peel_results = new EdgePeel[size];

      // Do the peeling
      int answer = Peel(edges, nodes, peel_results);
      if (answer < size) { // if peeling failed to peel all the way (found a 2-core)
        size += 1;
        continue;
      }
      // populate the fingerprints
      Unpeel(edges, peel_results);
      break;
    }
  }

  public boolean FindHash32(int hash) { return false; }


}
