#pragma once

#include <vector>

#include "graph/graph.h"

namespace cwz {

// An NSP candidate discovered during a reduction step. `cost` is the weight of
// the candidate s->t path; `path` is its edge sequence expressed in terms of
// the edges of the graph that was INPUT to the reduction that produced it.
struct ReductionCandidate {
    Weight cost;
    std::vector<EdgeId> path;
};

// Reduction-step output. The reduced graph G' shares vertex IDs with the
// original up to remapping; `vertex_old_to_new[v]` is the new ID of vertex v
// in g_prime, or kNoVertex if v was folded away. `path_expand[e]` records, for
// each edge e' in g_prime, the list of edges in g that the edge stands in for
// (a single edge if e' is preserved, or [edge into folded vertex, edge out of
// folded vertex] if e' is a shortcut).
//
// `candidates` collects NSP candidates that the reduction discovers and removes
// from the graph (the paper's NextSP-Straight records, for each removed
// forward-or-sideways back-edge (u,v), the candidate path
// Ps->u o (u,v) o Pv->t). These must be folded into the final answer alongside
// whatever NextSP-Layered finds on g_prime. Paths are in INPUT-graph edge terms.
struct ReductionResult {
    Graph g_prime;
    VertexId s_prime;
    VertexId t_prime;
    std::vector<VertexId> vertex_old_to_new;  // size = original n
    std::vector<std::vector<EdgeId>> path_expand;  // size = g_prime.num_edges()
    std::vector<ReductionCandidate> candidates;
};

// Reduce a general digraph (G, s, t) to an (s,t)-straight graph: every vertex
// lies on some shortest s -> t path. We do this in one pass: compute dS, dT,
// remove all vertices off any shortest path, and for each ordered pair (x, y)
// of (in-neighbor of folded vertex u, out-neighbor of folded vertex u) keep
// the minimum-weight shortcut (or original edge, whichever is shorter).
//
// Correctness: For NSP, what matters is the cost of the cheapest
// next-to-shortest s->t simple *path*. If vertex u is not on any shortest path,
// then any simple s->t path through u has cost >= dS[u] + dT[u] > dS(t).
// Replacing u by min-weight in-out shortcuts preserves all such paths' costs.
//
// Returns ReductionResult: the reduced graph plus mappings to recover paths.
ReductionResult reduce_to_straight(const Graph& g, VertexId s, VertexId t);

// Reduce an (s,t)-straight graph to a STRICTLY (s,t)-layered graph, following
// the paper's NextSP-Straight (Section 4.2). An (s,t)-layered graph satisfies
// Definition 4.1: every vertex on a shortest s->t path; no edge has equal-
// distance endpoints; and for every edge (u,v) with d(s,u)<d(s,v), no vertex
// has distance strictly between d(s,u) and d(s,u)+w(u,v).
//
// Two transformations make this hold:
//   * Forward edge (d(s,u)+w == d(s,v)) skipping layers -> subdivide into a
//     chain of single-layer forward edges.
//   * Back-edge with d(s,u) <= d(s,v) (forward-or-sideways in distance, hence
//     violating condition 2 or 3) -> REMOVE it, and record the candidate path
//     Ps->u o (u,v) o Pv->t (a valid simple not-shortest path) in
//     result.candidates. Strictly-backward back-edges (d(s,u) > d(s,v)) are
//     kept; they are exactly the back-edges the layered solver reasons about.
ReductionResult reduce_to_layered(const Graph& g, VertexId s, VertexId t);

// Pipeline: g -> straight -> layered.
ReductionResult reduce_full(const Graph& g, VertexId s, VertexId t);

}  // namespace cwz
