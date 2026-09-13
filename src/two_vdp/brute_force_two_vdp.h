#pragma once

#include <cstdint>
#include <vector>

#include "graph/graph.h"

namespace cwz {

// Exponential brute-force oracle for paired 2-VDP on any digraph.
// Requires num_vertices() <= 64 (else std::length_error).

// Bit v is set iff vertex v is in the set.
using VertexMask = std::uint64_t;

// Returns all simple s->t paths in g as vertex sets (endpoints included).
std::vector<VertexMask> brute_force_path_vertex_sets(const Graph& g, VertexId s,
                                                     VertexId t);

// True iff some set in `a` is disjoint from some set in `b`.
bool any_disjoint_vertex_sets(const std::vector<VertexMask>& a,
                              const std::vector<VertexMask>& b);

// Same contract as two_vdp_in_dag, but the graph may be cyclic.
bool brute_force_two_vdp_feasible(const Graph& g, VertexId s1, VertexId t1,
                                  VertexId s2, VertexId t2);

}  // namespace cwz
