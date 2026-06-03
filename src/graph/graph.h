#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace cwz {

using Weight = std::int64_t;
using VertexId = std::int32_t;
using EdgeId = std::int32_t;

inline constexpr Weight kInfWeight = std::numeric_limits<Weight>::max() / 4;
inline constexpr VertexId kNoVertex = -1;
inline constexpr EdgeId kNoEdge = -1;

struct Edge {
    EdgeId id;
    VertexId src;
    VertexId dst;
    Weight w;
};

// Directed graph with positive edge weights. Vertex IDs are dense [0, n).
// Edges have stable IDs assigned at insertion time. Both out- and in-adjacency
// lists are maintained so that reverse Dijkstra is O(m log n) without rebuilding.
class Graph {
   public:
    Graph() = default;
    explicit Graph(VertexId n);

    VertexId num_vertices() const { return static_cast<VertexId>(out_.size()); }
    EdgeId num_edges() const { return static_cast<EdgeId>(edges_.size()); }

    VertexId add_vertex();
    EdgeId add_edge(VertexId u, VertexId v, Weight w);

    const Edge& edge(EdgeId e) const { return edges_[e]; }
    const std::vector<EdgeId>& out_edges(VertexId v) const { return out_[v]; }
    const std::vector<EdgeId>& in_edges(VertexId v) const { return in_[v]; }

    // DIMACS-style I/O. First line: "n m". Following m lines: "u v w".
    // Vertices are 0-indexed.
    static Graph from_text(const std::string& text);
    std::string to_text() const;

   private:
    std::vector<Edge> edges_;
    std::vector<std::vector<EdgeId>> out_;
    std::vector<std::vector<EdgeId>> in_;
};

}  // namespace cwz
