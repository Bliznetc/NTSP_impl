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

// Brute-force NSP oracle: DFS over simple s->t paths with an admissible A*
// bound. Exponential with many equal-cost shortest paths. Throws
// std::length_error if num_vertices() > vertex_cap.
NspResult brute_force_nsp(const Graph& g, VertexId s, VertexId t,
                          VertexId vertex_cap = 30);

}  // namespace cwz
