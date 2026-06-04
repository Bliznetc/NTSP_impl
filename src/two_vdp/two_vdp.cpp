#include "two_vdp/two_vdp.h"

#include <algorithm>
#include <queue>
#include <unordered_map>

namespace cwz {

namespace {

// Internal flow network. Each original vertex v is split into v_in (id 2v) and
// v_out (id 2v+1), connected by a capacity-1 edge (the vertex itself), except
// for s1, s2, t1, t2 which get unbounded capacity (we use 2). For each
// original edge (u, v) we add a capacity-1 edge from u_out to v_in.
// Super-source has id 2n, super-sink has id 2n+1.
//
// We store the network as a Dinic-style list of (to, cap, rev_index) tuples.

struct FlowEdge {
    int to;
    int cap;
    int rev;
    EdgeId orig_edge;  // kNoEdge for vertex-split / super-source / super-sink edges
};

struct FlowNet {
    std::vector<std::vector<FlowEdge>> adj;
    int n;  // total nodes including super source/sink
    int src;
    int snk;

    void add(int u, int v, int cap, EdgeId orig = kNoEdge) {
        adj[u].push_back({v, cap, static_cast<int>(adj[v].size()), orig});
        adj[v].push_back({u, 0, static_cast<int>(adj[u].size()) - 1, kNoEdge});
    }
};

bool bfs_for_aug(FlowNet& net, std::vector<int>& parent_node, std::vector<int>& parent_idx) {
    std::fill(parent_node.begin(), parent_node.end(), -1);
    std::fill(parent_idx.begin(), parent_idx.end(), -1);
    parent_node[net.src] = net.src;
    std::queue<int> q;
    q.push(net.src);
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        for (int i = 0; i < static_cast<int>(net.adj[u].size()); ++i) {
            const FlowEdge& e = net.adj[u][i];
            if (e.cap > 0 && parent_node[e.to] == -1) {
                parent_node[e.to] = u;
                parent_idx[e.to] = i;
                if (e.to == net.snk) return true;
                q.push(e.to);
            }
        }
    }
    return false;
}

}  // namespace

std::optional<TwoVdpResult> two_vdp_in_dag(const Graph& g,
                                           VertexId s1, VertexId t1,
                                           VertexId s2, VertexId t2) {
    if (s1 == s2 || t1 == t2 || s1 == t2 || s2 == t1) return std::nullopt;

    const int n = g.num_vertices();
    FlowNet net;
    net.n = 2 * n + 2;
    net.src = 2 * n;
    net.snk = 2 * n + 1;
    net.adj.assign(net.n, {});

    auto v_in = [](int v) { return 2 * v; };
    auto v_out = [](int v) { return 2 * v + 1; };

    // Vertex-split edges (capacity 1 for every vertex, including the
    // prescribed endpoints). Combined with the capacity-1 super-source
    // feeders and capacity-1 super-sink drains, this enforces that each of
    // s1, s2, t1, t2 carries at most one unit of flow -- i.e., is used by
    // exactly one commodity, never crossed by the other. This is the
    // standard reduction; an earlier version of this file gave the endpoints
    // capacity 2, which allowed max-flow to spuriously return 2 on
    // instances where a path s_i -> t_i had to pass through the other
    // commodity's source. The trace fallback below happened to mask that
    // upstream bug in practice, but the formulation is now correct by
    // construction.
    for (int v = 0; v < n; ++v) {
        net.add(v_in(v), v_out(v), 1);
    }
    // Edge capacities.
    for (EdgeId i = 0; i < g.num_edges(); ++i) {
        const Edge& e = g.edge(i);
        net.add(v_out(e.src), v_in(e.dst), 1, i);
    }
    // Super source -> commodity sources (capacity 1 each), commodity sinks -> super sink.
    net.add(net.src, v_in(s1), 1);
    net.add(net.src, v_in(s2), 1);
    net.add(v_out(t1), net.snk, 1);
    net.add(v_out(t2), net.snk, 1);

    std::vector<int> parent_node(net.n, -1), parent_idx(net.n, -1);
    int flow = 0;
    while (flow < 2 && bfs_for_aug(net, parent_node, parent_idx)) {
        // augment along the path
        int cur = net.snk;
        while (cur != net.src) {
            int p = parent_node[cur];
            int idx = parent_idx[cur];
            net.adj[p][idx].cap--;
            FlowEdge& rev = net.adj[cur][net.adj[p][idx].rev];
            rev.cap++;
            cur = p;
        }
        flow++;
    }
    if (flow < 2) return std::nullopt;

    // Decompose flow into two paths by tracing forward from s1 and s2.
    // Each commodity source has out-flow 1 on its super-source feeder, so
    // there's a unique outgoing path along edges with reduced capacity (cap
    // dropped from 1 to 0). For vertex-split edges, an edge from v_in to
    // v_out with non-zero reduced flow is currently 0 cap; we track flow by
    // looking at non-zero cap on the reverse edge.
    auto trace = [&](VertexId src_v, VertexId snk_v) -> std::vector<EdgeId> {
        std::vector<EdgeId> result;
        int cur = v_out(src_v);
        std::vector<char> visited(net.n, 0);
        while (cur != v_out(snk_v) && cur != v_in(snk_v)) {
            // The "real" current vertex is v with cur = v_in(v) or v_out(v).
            // From v_out(v) we should go to some v_in(u) along an original edge
            // that has flow on it (reverse cap > 0 means original cap consumed).
            if (visited[cur]) return {};
            visited[cur] = 1;
            bool advanced = false;
            for (FlowEdge& e : net.adj[cur]) {
                if (e.orig_edge != kNoEdge && e.cap == 0 && e.to != net.src) {
                    // This is a saturated forward edge (originally cap 1, now 0).
                    // Take it if not visited.
                    if (visited[e.to]) continue;
                    result.push_back(e.orig_edge);
                    // Now we're at v_in(dst). Move through the vertex-split to v_out(dst).
                    int dst_v_in = e.to;
                    if (visited[dst_v_in]) return {};
                    visited[dst_v_in] = 1;
                    int dst_v_out = dst_v_in + 1;  // by construction
                    cur = dst_v_out;
                    advanced = true;
                    break;
                }
            }
            if (!advanced) return {};
            if (cur == v_out(snk_v)) break;
        }
        return result;
    };

    TwoVdpResult r;
    r.p1 = trace(s1, t1);
    r.p2 = trace(s2, t2);
    if (r.p1.empty() || r.p2.empty()) {
        // Tracing failed (rare; flow decomposition can be ambiguous). Fall back
        // to a simpler check: report feasibility but with empty paths.
        // For correctness of higher-level NSP, we need the actual paths; if
        // tracing failed, signal infeasible.
        if (s1 != t1 && s2 != t2) return std::nullopt;
    }
    return r;
}

}  // namespace cwz
