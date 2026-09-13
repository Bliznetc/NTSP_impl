#include "two_vdp/two_vdp.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>

namespace cwz {

namespace {

// Topological position of each vertex (Kahn). Self-loops are ignored; any
// other cycle throws std::invalid_argument.
std::vector<VertexId> topological_positions(const Graph& g) {
    const VertexId n = g.num_vertices();
    std::vector<VertexId> indeg(n, 0);
    for (EdgeId i = 0; i < g.num_edges(); ++i) {
        const Edge& e = g.edge(i);
        if (e.src != e.dst) ++indeg[e.dst];
    }
    std::vector<VertexId> order;
    order.reserve(n);
    for (VertexId v = 0; v < n; ++v)
        if (indeg[v] == 0) order.push_back(v);
    for (std::size_t head = 0; head < order.size(); ++head) {
        VertexId u = order[head];
        for (EdgeId eid : g.out_edges(u)) {
            const Edge& e = g.edge(eid);
            if (e.src == e.dst) continue;
            if (--indeg[e.dst] == 0) order.push_back(e.dst);
        }
    }
    if (static_cast<VertexId>(order.size()) != n)
        throw std::invalid_argument("two_vdp_in_dag: graph is not acyclic");
    std::vector<VertexId> pos(n);
    for (VertexId i = 0; i < n; ++i) pos[order[i]] = i;
    return pos;
}

constexpr EdgeId kUnvisited = -1;
constexpr EdgeId kStartState = -2;

}  // namespace

std::optional<TwoVdpResult> two_vdp_in_dag(const Graph& g,
                                           VertexId s1, VertexId t1,
                                           VertexId s2, VertexId t2) {
    // A shared terminal makes the pair infeasible.
    if (s1 == s2 || t1 == t2 || s1 == t2 || s2 == t1) return std::nullopt;

    const VertexId n = g.num_vertices();
    auto in_range = [n](VertexId v) { return v >= 0 && v < n; };
    if (!in_range(s1) || !in_range(t1) || !in_range(s2) || !in_range(t2))
        throw std::out_of_range("two_vdp_in_dag: terminal out of range");

    const std::vector<VertexId> pos = topological_positions(g);

    if (s1 == t1 && s2 == t2) return TwoVdpResult{};

    const std::size_t N = static_cast<std::size_t>(n);
    if (N > std::numeric_limits<std::size_t>::max() / N)
        throw std::length_error("two_vdp_in_dag: state space too large");
    auto idx = [N](VertexId u, VertexId v) {
        return static_cast<std::size_t>(u) * N + static_cast<std::size_t>(v);
    };

    // parent[idx(u,v)]: edge that first reached state (u,v).
    std::vector<EdgeId> parent(N * N, kUnvisited);
    std::vector<std::size_t> stack;
    const std::size_t start = idx(s1, s2);
    const std::size_t target = idx(t1, t2);
    parent[start] = kStartState;
    stack.push_back(start);

    bool found = false;
    while (!stack.empty() && !found) {
        const std::size_t cur = stack.back();
        stack.pop_back();
        const VertexId u = static_cast<VertexId>(cur / N);
        const VertexId v = static_cast<VertexId>(cur % N);

        // Advance the head earlier in topological order (a head at its target stays).
        // The other path's earlier vertices all precede this head, so only its
        // current head can collide.
        const bool move_p1 = (u != t1) && (v == t2 || pos[u] < pos[v]);
        const VertexId head = move_p1 ? u : v;
        const VertexId other = move_p1 ? v : u;
        for (EdgeId eid : g.out_edges(head)) {
            const VertexId w = g.edge(eid).dst;
            if (w == head || w == other) continue;
            const std::size_t nxt = move_p1 ? idx(w, v) : idx(u, w);
            if (parent[nxt] != kUnvisited) continue;
            parent[nxt] = eid;
            if (nxt == target) {
                found = true;
                break;
            }
            stack.push_back(nxt);
        }
    }
    if (!found) return std::nullopt;

    TwoVdpResult r;
    VertexId u = t1, v = t2;
    for (EdgeId eid = parent[target]; eid != kStartState; eid = parent[idx(u, v)]) {
        const Edge& e = g.edge(eid);
        if (e.dst == u) {
            r.p1.push_back(eid);
            u = e.src;
        } else {
            r.p2.push_back(eid);
            v = e.src;
        }
    }
    std::reverse(r.p1.begin(), r.p1.end());
    std::reverse(r.p2.begin(), r.p2.end());
    return r;
}

}  // namespace cwz
