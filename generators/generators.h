#pragma once

#include <cstdint>
#include <random>

#include "graph/graph.h"

namespace cwz::gen {

// Each ordered pair u != v is an edge with probability p; weights in [1, max_weight].
Graph erdos_renyi(VertexId n, double p, Weight max_weight, std::mt19937& rng);

// Edge u->v (u < v) with probability p.
Graph random_dag(VertexId n, double p, Weight max_weight, std::mt19937& rng);

// s = 0, t = n-1, `layers` x `width` interior vertices. Forward edges between
// adjacent layers w.p. p_forward (t always reachable), back-edges w.p. p_back.
Graph layered(int layers, int width, double p_forward, double p_back,
              Weight max_weight, std::mt19937& rng);

// rows x cols grid, edges right and down; s = (0,0), t = (rows-1, cols-1).
Graph grid(int rows, int cols, Weight max_weight, std::mt19937& rng);

// Barabási–Albert style: each new vertex links to m degree-weighted vertices.
Graph scale_free(VertexId n, int m, Weight max_weight, std::mt19937& rng);

// k diamonds in series (2^k shortest paths of cost 2kw) plus an s->t edge of
// cost 2kw + nsp_extra, the unique NSP. n = 1 + 3k, m = 4k + 1.
Graph diamond_chain(int k, Weight w, Weight nsp_extra);

}  // namespace cwz::gen
