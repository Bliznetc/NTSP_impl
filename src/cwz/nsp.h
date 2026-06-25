#pragma once

#include "baseline/brute_force.h"  // reuse NspResult
#include "graph/graph.h"

namespace cwz {

// Top-level Chen-Wein-Zhang next-to-shortest path on a directed graph with
// positive weights. Steps:
//   1. Reduce G to (s,t)-straight (drop vertices off all shortest paths).
//   2. Reduce to (s,t)-layered (subdivide layer-skipping forward edges).
//   3. Run the layered NSP algorithm with the 6-tuple enumeration of Lemma 5.3.
//   4. Map the result back to edges of the original graph.
//
// Returns cost == kInfWeight if no NSP exists.
//
// IMPLEMENTATION NOTE: the layered algorithm here matches the *structure* of
// the paper's Algorithm 1 (Section 5) — enumerate 6-tuples, two 2-VDP queries,
// shortest path in residual — but uses a max-flow based 2-VDP-in-DAG rather
// than Tholey's linear-time algorithm. Correctness is unaffected; runtime
// matches O(|V|^4 |E|^3 \cdot (|V|+|E|)) bounds rather than the paper's tight
// O(|V|^4 |E|^3 log |V|).
NspResult cwz_nsp(const Graph& g, VertexId s, VertexId t);

// Run only the layered algorithm on an already-(s,t)-layered graph. Exposed
// for testing.
NspResult cwz_nsp_layered(const Graph& g, VertexId s, VertexId t);

}  // namespace cwz
