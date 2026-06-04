#include "baseline/yen.h"

#include <algorithm>
#include <cassert>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "shortest_path/dijkstra.h"

namespace cwz {

namespace {

// Dijkstra restricted to edges and vertices not in the given masks. The masks
// are passed as bitmaps over edge IDs and vertex IDs.
DijkstraResult dijkstra_masked(const Graph& g, VertexId src,
                               const std::vector<char>& blocked_vertex,
                               const std::vector<char>& blocked_edge) {
    const VertexId n = g.num_vertices();
    DijkstraResult r{std::vector<Weight>(n, kInfWeight), std::vector<EdgeId>(n, kNoEdge)};
    if (blocked_vertex[src]) return r;
    r.dist[src] = 0;
    struct PqEntry {
        Weight d;
        VertexId v;
        bool operator>(const PqEntry& o) const { return d > o.d; }
    };
    std::priority_queue<PqEntry, std::vector<PqEntry>, std::greater<>> pq;
    pq.push({0, src});
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d != r.dist[u]) continue;
        for (EdgeId eid : g.out_edges(u)) {
            if (blocked_edge[eid]) continue;
            const Edge& e = g.edge(eid);
            if (blocked_vertex[e.dst]) continue;
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

// Recover the path (as edge list) from a Dijkstra parent array.
std::vector<EdgeId> recover_path(const Graph& g, const DijkstraResult& r,
                                 VertexId src, VertexId tgt) {
    std::vector<EdgeId> path;
    VertexId cur = tgt;
    while (cur != src) {
        EdgeId eid = r.parent[cur];
        if (eid == kNoEdge) return {};
        path.push_back(eid);
        cur = g.edge(eid).src;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

Weight path_cost(const Graph& g, const std::vector<EdgeId>& edges) {
    Weight c = 0;
    for (EdgeId eid : edges) c += g.edge(eid).w;
    return c;
}

}  // namespace

NspResult yen_nsp(const Graph& g, VertexId s, VertexId t, int k_max) {
    NspResult r;
    if (s == t) return r;
    const VertexId n = g.num_vertices();

    auto first = dijkstra(g, s);
    if (first.dist[t] >= kInfWeight) return r;
    r.shortest_cost = first.dist[t];

    std::vector<std::vector<EdgeId>> A;
    A.push_back(recover_path(g, first, s, t));

    // Invariant: the path recovered by walking `first.parent[]` backwards from
    // t to s must sum exactly to `first.dist[t]`. If this assert ever fires,
    // there is a bug in Dijkstra (probably dist[] and parent[] are out of sync).
    // Verified at the Dijkstra layer by Dijkstra.RecoveredPathCostMatchesDist.
    assert(path_cost(g, A.front()) == r.shortest_cost);

    // Candidate paths, ordered by cost. We deduplicate by edge sequence.
    struct Candidate {
        Weight cost;
        std::vector<EdgeId> edges;
        bool operator>(const Candidate& o) const { return cost > o.cost; }
    };
    std::priority_queue<Candidate, std::vector<Candidate>, std::greater<>> B;
    std::set<std::vector<EdgeId>> seen;
    seen.insert(A.front());

    std::vector<char> blocked_vertex(n, 0);
    std::vector<char> blocked_edge(g.num_edges(), 0);

    for (int k = 1; k < k_max; ++k) {
        const std::vector<EdgeId>& prev = A.back();
        std::vector<EdgeId> root_path;
        root_path.reserve(prev.size());
        // Vertices on the root path (excluding spur node), to block in the spur Dijkstra.
        std::vector<VertexId> root_vertices;
        root_vertices.push_back(s);

        for (std::size_t i = 0; i < prev.size(); ++i) {
            VertexId spur_node = g.edge(prev[i]).src;
            // Block edges that are the i-th edge of some path in A whose root path matches.
            std::vector<EdgeId> just_blocked_edges;
            for (const auto& path : A) {
                if (path.size() <= i) continue;
                bool match = true;
                for (std::size_t j = 0; j < i; ++j) {
                    if (path[j] != root_path[j]) { match = false; break; }
                }
                if (match && !blocked_edge[path[i]]) {
                    blocked_edge[path[i]] = 1;
                    just_blocked_edges.push_back(path[i]);
                }
            }
            for (VertexId v : root_vertices) blocked_vertex[v] = 1;
            blocked_vertex[spur_node] = 0;  // spur node itself must remain available

            auto spur_dij = dijkstra_masked(g, spur_node, blocked_vertex, blocked_edge);
            if (spur_dij.dist[t] < kInfWeight) {
                auto spur_path = recover_path(g, spur_dij, spur_node, t);
                std::vector<EdgeId> total = root_path;
                total.insert(total.end(), spur_path.begin(), spur_path.end());
                if (seen.insert(total).second) {
                    B.push({path_cost(g, total), std::move(total)});
                }
            }

            for (VertexId v : root_vertices) blocked_vertex[v] = 0;
            for (EdgeId e : just_blocked_edges) blocked_edge[e] = 0;

            root_path.push_back(prev[i]);
            root_vertices.push_back(g.edge(prev[i]).dst);
        }

        if (B.empty()) return r;  // no more simple s->t paths exist; no NSP

        auto next = B.top();
        B.pop();
        if (next.cost > r.shortest_cost) {
            r.cost = next.cost;
            r.edges = std::move(next.edges);
            return r;
        }
        A.push_back(std::move(next.edges));
    }
    // Hit the cap without finding a strictly longer path. False-negative.
    return r;
}

}  // namespace cwz
