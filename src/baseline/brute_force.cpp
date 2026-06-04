#include "baseline/brute_force.h"

#include <stdexcept>

namespace cwz {

namespace {

struct DfsState {
    const Graph& g;
    VertexId t;
    Weight shortest;
    Weight best;
    std::vector<EdgeId> best_path;
    std::vector<char> on_path;
    std::vector<EdgeId> path_edges;
};

void dfs(DfsState& st, VertexId u, Weight cost) {
    if (u == st.t) {
        // Strictly greater than the shortest path cost, strictly less than best.
        if (cost > st.shortest && cost < st.best) {
            st.best = cost;
            st.best_path = st.path_edges;
        }
        return;
    }
    if (cost >= st.best) return;  // can't improve
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

struct ShortestDfsState {
    const Graph& g;
    VertexId t;
    Weight best;
    std::vector<char> on_path;
};

void shortest_dfs(ShortestDfsState& st, VertexId u, Weight cost) {
    if (u == st.t) {
        if (cost < st.best) st.best = cost;
        return;
    }
    if (cost >= st.best) return;  // can't improve
    for (EdgeId eid : st.g.out_edges(u)) {
        const Edge& e = st.g.edge(eid);
        if (st.on_path[e.dst]) continue;
        st.on_path[e.dst] = 1;
        shortest_dfs(st, e.dst, cost + e.w);
        st.on_path[e.dst] = 0;
    }
}

Weight shortest_cost(const Graph& g, VertexId s, VertexId t) {
    // Recursive DFS over simple s->t paths with branch-and-bound on `best`.
    // Same exponential worst case as Dijkstra-via-brute-force, but
    // self-contained and structurally identical to dfs() above. We could
    // call Dijkstra here for an O((V+E) log V) shortcut; we don't because
    // brute_force.cpp is meant to be a minimal-dependency oracle.
    ShortestDfsState st{g, t, kInfWeight, std::vector<char>(g.num_vertices(), 0)};
    st.on_path[s] = 1;
    shortest_dfs(st, s, 0);
    return st.best;
}

}  // namespace

NspResult brute_force_nsp(const Graph& g, VertexId s, VertexId t, VertexId vertex_cap) {
    if (g.num_vertices() > vertex_cap) {
        throw std::length_error(
            "brute_force_nsp: graph exceeds vertex_cap (raise it explicitly to override)");
    }
    NspResult r;
    if (s == t) return r;
    r.shortest_cost = shortest_cost(g, s, t);
    if (r.shortest_cost >= kInfWeight) return r;

    DfsState st{g, t, r.shortest_cost, kInfWeight, {}, std::vector<char>(g.num_vertices(), 0), {}};
    st.on_path[s] = 1;
    dfs(st, s, 0);

    if (st.best < kInfWeight) {
        r.cost = st.best;
        r.edges = std::move(st.best_path);
    }
    return r;
}

}  // namespace cwz
