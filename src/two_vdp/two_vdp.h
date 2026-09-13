#pragma once

#include <optional>
#include <vector>

#include "graph/graph.h"

namespace cwz {

// Paired 2-vertex-disjoint paths in a DAG (Fortune-Hopcroft-Wyllie, k = 2).
// Returns disjoint P1: s1->t1 and P2: s2->t2, or nullopt. Throws
// std::invalid_argument on a cycle, std::out_of_range on bad terminals.
// O(n^2 + n*m) time, O(n^2) memory.
struct TwoVdpResult {
    std::vector<EdgeId> p1;  // edges of P1, in order s1 -> t1
    std::vector<EdgeId> p2;  // edges of P2, in order s2 -> t2
};

std::optional<TwoVdpResult> two_vdp_in_dag(const Graph& g,
                                           VertexId s1, VertexId t1,
                                           VertexId s2, VertexId t2);

}  // namespace cwz
