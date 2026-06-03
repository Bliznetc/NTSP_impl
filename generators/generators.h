#pragma once

#include <cstdint>
#include <random>

#include "graph/graph.h"

namespace cwz::gen {

// Erdős–Rényi style random digraph: each ordered pair (u, v) with u != v has
// probability p of being an edge. Weights are uniform in [1, max_weight].
Graph erdos_renyi(VertexId n, double p, Weight max_weight, std::mt19937& rng);

// Random DAG: vertices are 0..n-1 in topological order; an edge u->v is added
// only if u < v, each such pair with probability p. Guarantees acyclicity.
Graph random_dag(VertexId n, double p, Weight max_weight, std::mt19937& rng);

// Layered digraph: `layers` layers of `width` vertices each, plus s (vertex 0)
// and t (vertex n-1). Each vertex on layer i has each vertex on layer i+1 as a
// successor with probability p_forward. Optional p_back adds back-edges
// (i+1 -> i) with that probability.
Graph layered(int layers, int width, double p_forward, double p_back,
              Weight max_weight, std::mt19937& rng);

// Grid digraph: rows x cols grid, edges go right and down. s = (0,0), t = (rows-1, cols-1).
Graph grid(int rows, int cols, Weight max_weight, std::mt19937& rng);

// Scale-free preferential attachment (Barabási–Albert style). Each new vertex
// connects to `m` existing vertices chosen with probability proportional to
// in-degree+out-degree. Edge direction is randomized.
Graph scale_free(VertexId n, int m, Weight max_weight, std::mt19937& rng);

// Adversarial "diamond chain": k parallel diamonds connecting s to t, each
// offering 2 vertex-disjoint single-hop alternatives of equal weight w. The
// graph has exactly 2^k distinct simple shortest s->t paths, all of weight
// 2*k*w. A single "shortcut" edge s -> t of weight 2*k*w + nsp_extra creates
// a unique NSP. Designed to stress Yen-NSP, which must enumerate all 2^k
// shortest paths before finding the NSP; CWZ's runtime depends on graph size
// rather than path multiplicity.
//
// Vertex layout: s=0; for each diamond i in 1..k vertices 3i-2 (top), 3i-1
// (bot), 3i (merge). Total vertices: 1 + 3k. Total edges: 4k + 1.
Graph diamond_chain(int k, Weight w, Weight nsp_extra);

}  // namespace cwz::gen
