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

// Brute-force next-to-shortest path. Enumerates all simple s->t paths via DFS
// and returns the cheapest one whose cost is strictly greater than the shortest
// path cost. Intended as a correctness oracle for the CWZ implementation;
// only practical for n <= ~15.
//
// `vertex_cap` (default 30) is a hard safety limit: if g.num_vertices() exceeds
// it, the function throws std::length_error. Set explicitly to override.
NspResult brute_force_nsp(const Graph& g, VertexId s, VertexId t,
                          VertexId vertex_cap = 30);

}  // namespace cwz
