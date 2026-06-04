#pragma once

#include "baseline/brute_force.h"
#include "graph/graph.h"

namespace cwz {

// Yen's k-shortest-loopless-paths used as an NSP heuristic. We enumerate simple
// s->t paths in non-decreasing order of cost and return the first one whose cost
// strictly exceeds the shortest path's cost (i.e. the NSP).
//
// Yen produces *all* simple shortest paths before producing a strictly longer
// one, so when many shortest paths exist this can be exponentially slow. The
// `k_max` parameter caps the enumeration; if we hit the cap without finding a
// strictly longer path, the function returns NspResult{} (no NSP found from
// this run — possibly a false negative).
NspResult yen_nsp(const Graph& g, VertexId s, VertexId t, int k_max = 10000);

}  // namespace cwz
