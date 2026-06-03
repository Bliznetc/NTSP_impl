#include "graph/graph.h"

#include <cassert>
#include <sstream>
#include <stdexcept>

namespace cwz {

Graph::Graph(VertexId n) : out_(n), in_(n) {}

VertexId Graph::add_vertex() {
    VertexId id = static_cast<VertexId>(out_.size());
    out_.emplace_back();
    in_.emplace_back();
    return id;
}

EdgeId Graph::add_edge(VertexId u, VertexId v, Weight w) {
    assert(u >= 0 && u < num_vertices());
    assert(v >= 0 && v < num_vertices());
    if (w <= 0) throw std::invalid_argument("Graph::add_edge requires positive weight");
    EdgeId id = static_cast<EdgeId>(edges_.size());
    edges_.push_back({id, u, v, w});
    out_[u].push_back(id);
    in_[v].push_back(id);
    return id;
}

Graph Graph::from_text(const std::string& text) {
    std::istringstream is(text);
    VertexId n;
    EdgeId m;
    if (!(is >> n >> m)) throw std::invalid_argument("Graph::from_text: bad header");
    Graph g(n);
    for (EdgeId i = 0; i < m; ++i) {
        VertexId u, v;
        Weight w;
        if (!(is >> u >> v >> w))
            throw std::invalid_argument("Graph::from_text: bad edge line");
        g.add_edge(u, v, w);
    }
    return g;
}

std::string Graph::to_text() const {
    std::ostringstream os;
    os << num_vertices() << ' ' << num_edges() << '\n';
    for (const Edge& e : edges_) {
        os << e.src << ' ' << e.dst << ' ' << e.w << '\n';
    }
    return os.str();
}

}  // namespace cwz
