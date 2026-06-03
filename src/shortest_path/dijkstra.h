#pragma once

#include <vector>

#include "graph/graph.h"

namespace cwz {

struct DijkstraResult {
    std::vector<Weight> dist;     // dist[v] = shortest distance from source
    std::vector<EdgeId> parent;   // parent edge on a shortest path; kNoEdge for source / unreachable
};

// Single-source shortest paths from `src` over forward edges (u -> v).
// Unreachable vertices have dist = kInfWeight, parent = kNoEdge.
DijkstraResult dijkstra(const Graph& g, VertexId src);

// Single-source shortest paths to `tgt` over reverse edges (v -> u).
// Equivalently: dist[v] = shortest distance from v to tgt.
DijkstraResult dijkstra_reverse(const Graph& g, VertexId tgt);

}  // namespace cwz
