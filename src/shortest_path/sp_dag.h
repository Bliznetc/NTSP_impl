#pragma once

#include <vector>

#include "graph/graph.h"
#include "shortest_path/dijkstra.h"

namespace cwz {

// Edges and vertices on some shortest s->t path:
//   edge (u,v,w): dS[u] + w + dT[v] == dS[t];  vertex v: dS[v] + dT[v] == dS[t].
// Empty if t is unreachable.
struct ShortestPathDag {
    std::vector<EdgeId> edges_on_sp;
    std::vector<char> vertex_on_sp;
    Weight st_distance = kInfWeight;  // dS[t]; kInfWeight if t unreachable
};

ShortestPathDag build_sp_dag(const Graph& g, VertexId s, VertexId t,
                             const DijkstraResult& from_s,
                             const DijkstraResult& to_t);

inline ShortestPathDag build_sp_dag(const Graph& g, VertexId s, VertexId t) {
    return build_sp_dag(g, s, t, dijkstra(g, s), dijkstra_reverse(g, t));
}

}  // namespace cwz
