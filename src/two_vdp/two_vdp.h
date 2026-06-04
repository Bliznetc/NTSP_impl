#pragma once

#include <optional>
#include <vector>

#include "graph/graph.h"

namespace cwz {

// 2-Vertex-Disjoint-Paths in a DAG.
//
// Given a DAG G (the caller passes a graph already known to be acyclic;
// the algorithm does not verify this) and two source-sink pairs (s1, t1) and
// (s2, t2), decide if there exist vertex-disjoint paths P1: s1 -> t1 and
// P2: s2 -> t2. Interior vertices of P1 must be disjoint from those of P2;
// the four endpoints s1, t1, s2, t2 are also vertex-distinct from each other
// in the canonical use (the caller is responsible for ensuring distinct
// endpoints).
//
// We solve this via vertex-capacity-1 max flow in the DAG with a super source
// connecting to {s1, s2} (each with capacity 1) and a super sink receiving from
// {t1, t2} (each with capacity 1). If max flow = 2, we extract two vertex-
// disjoint paths from the flow decomposition.
//
// Important: this is the standard "2-commodity vertex-disjoint paths" problem
// reduction to single-commodity flow with a super-source/sink. It correctly
// solves the problem only when each commodity's source is distinct from the
// other's sink — i.e., t1 != s2 and t2 != s1. The caller's intended use
// (lower-half 2-VDP for P1: s->X', P2: B->Y', and upper-half 2-VDP for
// P1: X->A, P2: Y->t) satisfies these conditions because of the layer
// constraints.
//
// Implementation note: this is NOT Tholey's linear-time algorithm — it uses
// max-flow, costing O(V * (V + E)) per call. Correctness is the same; the
// runtime constant is higher than the paper's tight bound.
//
// Returns std::nullopt if no feasible disjoint pair exists.
struct TwoVdpResult {
    std::vector<EdgeId> p1;  // edges of P1
    std::vector<EdgeId> p2;  // edges of P2
};

std::optional<TwoVdpResult> two_vdp_in_dag(const Graph& g,
                                           VertexId s1, VertexId t1,
                                           VertexId s2, VertexId t2);

}  // namespace cwz
