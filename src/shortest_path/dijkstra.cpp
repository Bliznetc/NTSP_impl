#include "shortest_path/dijkstra.h"

#include <queue>
#include <utility>

namespace cwz {

namespace {

struct PqEntry {
    Weight dist;
    VertexId v;
    bool operator>(const PqEntry& o) const { return dist > o.dist; }
};

}  // namespace

DijkstraResult dijkstra(const Graph& g, VertexId src) {
    const VertexId n = g.num_vertices();
    DijkstraResult r{std::vector<Weight>(n, kInfWeight), std::vector<EdgeId>(n, kNoEdge)};
    r.dist[src] = 0;
    std::priority_queue<PqEntry, std::vector<PqEntry>, std::greater<>> pq;
    pq.push({0, src});
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d != r.dist[u]) continue;
        for (EdgeId eid : g.out_edges(u)) {
            const Edge& e = g.edge(eid);
            Weight nd = d + e.w;
            if (nd < r.dist[e.dst]) {
                r.dist[e.dst] = nd;
                r.parent[e.dst] = eid;
                pq.push({nd, e.dst});
            }
        }
    }
    return r;
}

DijkstraResult dijkstra_reverse(const Graph& g, VertexId tgt) {
    const VertexId n = g.num_vertices();
    DijkstraResult r{std::vector<Weight>(n, kInfWeight), std::vector<EdgeId>(n, kNoEdge)};
    r.dist[tgt] = 0;
    std::priority_queue<PqEntry, std::vector<PqEntry>, std::greater<>> pq;
    pq.push({0, tgt});
    while (!pq.empty()) {
        auto [d, v] = pq.top();
        pq.pop();
        if (d != r.dist[v]) continue;
        for (EdgeId eid : g.in_edges(v)) {
            const Edge& e = g.edge(eid);
            Weight nd = d + e.w;
            if (nd < r.dist[e.src]) {
                r.dist[e.src] = nd;
                r.parent[e.src] = eid;
                pq.push({nd, e.src});
            }
        }
    }
    return r;
}

}  // namespace cwz
