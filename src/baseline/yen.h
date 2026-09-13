#pragma once

#include "baseline/brute_force.h"
#include "graph/graph.h"

namespace cwz {

// NSP via Yen's k shortest loopless paths: the first path costlier than the
// shortest. Gives up after k_max paths (possible false negative).
NspResult yen_nsp(const Graph& g, VertexId s, VertexId t, int k_max = 10000);

}  // namespace cwz
