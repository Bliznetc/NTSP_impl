#pragma once

#include <vector>

#include "graph/graph.h"

namespace cwz {

// NSP candidate found by a reduction, in the reduction's input edges.
struct ReductionCandidate {
    Weight cost;
    std::vector<EdgeId> path;
};

// vertex_old_to_new[v]: new id of v, or kNoVertex if folded away.
// path_expand[e']: input edges that reduced edge e' stands for.
// candidates: NSP candidates from removed back-edges (input edges).
struct ReductionResult {
    Graph g_prime;
    VertexId s_prime;
    VertexId t_prime;
    std::vector<VertexId> vertex_old_to_new;  // size = original n
    std::vector<std::vector<EdgeId>> path_expand;  // size = g_prime.num_edges()
    std::vector<ReductionCandidate> candidates;
};

// Fold vertices on no shortest s->t path into shortcut edges, preserving
// the cost of every simple s->t path through them.
ReductionResult reduce_to_straight(const Graph& g, VertexId s, VertexId t);

// NextSP-Straight (Section 4.2): subdivide layer-skipping forward edges and
// remove back-edges with d(s,u) <= d(s,v), recording Ps->u o (u,v) o Pv->t
// as a candidate. Strictly backward back-edges are kept.
ReductionResult reduce_to_layered(const Graph& g, VertexId s, VertexId t);

// Pipeline: g -> straight -> layered.
ReductionResult reduce_full(const Graph& g, VertexId s, VertexId t);

}  // namespace cwz
