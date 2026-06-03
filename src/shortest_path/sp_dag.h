#pragma once

#include <vector>

#include "graph/graph.h"
#include "shortest_path/dijkstra.h"

namespace cwz {

// Identification of the shortest-path DAG of (G, s, t): the subgraph induced
// by edges that lie on some shortest s -> t path. By definition, an edge
// (u, v) of weight w is on some shortest s -> t path iff
//     dS[u] + w + dT[v] == dS[t]
// where dS = dist from s, dT = dist to t. A vertex v lies on some shortest
// s -> t path iff dS[v] + dT[v] == dS[t].
//
// Returned `edges_on_sp` lists edge IDs of the original graph in arbitrary order.
// `vertex_on_sp[v]` is true iff v lies on some shortest s -> t path. If t is
// not reachable from s, both are empty / all-false respectively.
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
