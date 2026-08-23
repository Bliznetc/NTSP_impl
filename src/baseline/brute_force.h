#pragma once

#include <optional>
#include <vector>

#include "graph/graph.h"

namespace cwz {

struct NspResult {
    Weight cost = kInfWeight;          // kInfWeight if no NSP exists
    std::vector<EdgeId> edges;         // edges of an NSP from s to t (empty if none)
    Weight shortest_cost = kInfWeight; // shortest s->t distance (informational)
};

// Brute-force next-to-shortest path. Searches simple s->t paths by DFS and
// returns the cheapest one whose cost is strictly greater than the shortest
// path cost. Intended as a correctness oracle for the CWZ implementation.
//
// The search is branch-and-bound, not exhaustive enumeration: a reverse
// Dijkstra supplies an admissible lower bound d(u,t), and any prefix with
// cost + d(u,t) >= best is pruned. In practice this keeps the oracle in the
// microseconds well past |V| = 50. The exception is graphs with very many
// equal-cost shortest paths (the diamond-chain family is built to be one):
// the bound cannot prune those, every shortest path must be walked, and the
// runtime is then exponential -- which is exactly the regime the CWZ
// algorithm exists to handle.
//
// `vertex_cap` (default 30) is a hard safety limit: if g.num_vertices() exceeds
// it, the function throws std::length_error. Set explicitly to override.
NspResult brute_force_nsp(const Graph& g, VertexId s, VertexId t,
                          VertexId vertex_cap = 30);

}  // namespace cwz
