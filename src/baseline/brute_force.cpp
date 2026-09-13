#include "baseline/brute_force.h"

#include <queue>
#include <stdexcept>
#include <utility>

namespace cwz {

namespace {

// Own Dijkstra, independent of src/shortest_path/ so the oracle shares no
// code with the code under test. `reverse` gives distances to src.
std::vector<Weight> dijkstra_local(const Graph& g, VertexId src, bool reverse) {
    const VertexId n = g.num_vertices();
    std::vector<Weight> dist(n, kInfWeight);
    using QE = std::pair<Weight, VertexId>;
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> pq;
    dist[src] = 0;
    pq.emplace(0, src);
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d != dist[u]) continue;
        if (reverse) {
            for (EdgeId eid : g.in_edges(u)) {
                const Edge& e = g.edge(eid);
                if (d + e.w < dist[e.src]) {
                    dist[e.src] = d + e.w;
                    pq.emplace(dist[e.src], e.src);
                }
            }
        } else {
            for (EdgeId eid : g.out_edges(u)) {
                const Edge& e = g.edge(eid);
                if (d + e.w < dist[e.dst]) {
                    dist[e.dst] = d + e.w;
                    pq.emplace(dist[e.dst], e.dst);
                }
            }
        }
    }
    return dist;
}

struct DfsState {
    const Graph& g;
    VertexId t;
    Weight shortest;
    Weight best;
    const std::vector<Weight>* to_t;  // shortest u->t distance; admissible bound
    std::vector<EdgeId> best_path;
    std::vector<char> on_path;
    std::vector<EdgeId> path_edges;
};

void dfs(DfsState& st, VertexId u, Weight cost) {
    if (u == st.t) {
        if (cost > st.shortest && cost < st.best) {
            st.best = cost;
            st.best_path = st.path_edges;
        }
        return;
    }
    // A* pruning: to_t[u] is an admissible bound on the remaining cost.
    const Weight h = (*st.to_t)[u];
    if (h >= kInfWeight) return;         // t not reachable from u
    if (cost + h >= st.best) return;     // cannot improve on best
    for (EdgeId eid : st.g.out_edges(u)) {
        const Edge& e = st.g.edge(eid);
        if (st.on_path[e.dst]) continue;
        st.on_path[e.dst] = 1;
        st.path_edges.push_back(eid);
        dfs(st, e.dst, cost + e.w);
        st.path_edges.pop_back();
        st.on_path[e.dst] = 0;
    }
}

}  // namespace

NspResult brute_force_nsp(const Graph& g, VertexId s, VertexId t, VertexId vertex_cap) {
    if (g.num_vertices() > vertex_cap) {
        throw std::length_error(
            "brute_force_nsp: graph exceeds vertex_cap (raise it explicitly to override)");
    }
    if (s < 0 || s >= g.num_vertices() || t < 0 || t >= g.num_vertices()) {
        throw std::out_of_range("brute_force_nsp: s or t out of range");
    }
    NspResult r;
    if (s == t) return r;

    const std::vector<Weight> to_t = dijkstra_local(g, t, /*reverse=*/true);
    r.shortest_cost = to_t[s];
    if (r.shortest_cost >= kInfWeight) return r;

    DfsState st{g,  t, r.shortest_cost, kInfWeight, &to_t, {},
                std::vector<char>(g.num_vertices(), 0), {}};
    st.on_path[s] = 1;
    dfs(st, s, 0);

    if (st.best < kInfWeight) {
        r.cost = st.best;
        r.edges = std::move(st.best_path);
    }
    return r;
}

}  // namespace cwz
