#pragma once

#include <vector>

#include "graph/graph.h"

namespace cwz {

struct DijkstraResult {
    std::vector<Weight> dist;     // dist[v] = shortest distance from source
    std::vector<EdgeId> parent;   // kNoEdge for source / unreachable
};

// Distances from src; unreachable vertices get kInfWeight.
DijkstraResult dijkstra(const Graph& g, VertexId src);

// Distances to tgt, over reversed edges.
DijkstraResult dijkstra_reverse(const Graph& g, VertexId tgt);

}  // namespace cwz
