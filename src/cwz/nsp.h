#pragma once

#include "baseline/brute_force.h"  // reuse NspResult
#include "graph/graph.h"

namespace cwz {

// Chen-Wein-Zhang NSP: reduce to (s,t)-straight, then (s,t)-layered, run the
// Lemma 5.3 6-tuple enumeration and map the path back to g.
// Returns cost == kInfWeight if no NSP exists.
NspResult cwz_nsp(const Graph& g, VertexId s, VertexId t);

// Layered algorithm only; g must already be (s,t)-layered.
NspResult cwz_nsp_layered(const Graph& g, VertexId s, VertexId t);

}  // namespace cwz
