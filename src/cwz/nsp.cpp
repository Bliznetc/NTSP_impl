#include "cwz/nsp.h"

#include <algorithm>
#include <queue>

#include "cwz/reductions.h"
#include "shortest_path/dijkstra.h"
#include "shortest_path/sp_dag.h"
#include "two_vdp/two_vdp.h"

namespace cwz {

namespace {

struct ForwardGraph {
    Graph fg;
    std::vector<EdgeId> map_to_original;
    std::vector<char> is_forward;
};

ForwardGraph build_forward(const Graph& g, const DijkstraResult& from_s) {
    ForwardGraph f;
    f.fg = Graph(g.num_vertices());
    f.is_forward.assign(g.num_edges(), 0);
    for (EdgeId i = 0; i < g.num_edges(); ++i) {
        const Edge& e = g.edge(i);
        if (from_s.dist[e.src] < kInfWeight && from_s.dist[e.dst] < kInfWeight &&
            from_s.dist[e.src] + e.w == from_s.dist[e.dst]) {
            EdgeId fi = f.fg.add_edge(e.src, e.dst, e.w);
            f.is_forward[i] = 1;
            if (fi >= static_cast<EdgeId>(f.map_to_original.size()))
                f.map_to_original.resize(fi + 1);
            f.map_to_original[fi] = i;
        }
    }
    return f;
}

std::vector<EdgeId> recover_path(const Graph& g, const std::vector<EdgeId>& parent,
                                 VertexId src, VertexId tgt) {
    std::vector<EdgeId> path;
    VertexId cur = tgt;
    while (cur != src) {
        EdgeId eid = parent[cur];
        if (eid == kNoEdge) return {};
        path.push_back(eid);
        cur = g.edge(eid).src;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

struct MaskedDij {
    std::vector<Weight> dist;
    std::vector<EdgeId> parent;
};

MaskedDij dijkstra_masked(const Graph& g, VertexId src,
                          const std::vector<char>& allowed_vertex) {
    const VertexId n = g.num_vertices();
    MaskedDij r{std::vector<Weight>(n, kInfWeight), std::vector<EdgeId>(n, kNoEdge)};
    if (!allowed_vertex[src]) return r;
    r.dist[src] = 0;
    struct E { Weight d; VertexId v; bool operator>(const E& o) const { return d > o.d; } };
    std::priority_queue<E, std::vector<E>, std::greater<>> pq;
    pq.push({0, src});
    while (!pq.empty()) {
        auto [d, u] = pq.top();
        pq.pop();
        if (d != r.dist[u]) continue;
        for (EdgeId eid : g.out_edges(u)) {
            const Edge& e = g.edge(eid);
            if (!allowed_vertex[e.dst]) continue;
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

// Complete disjoint Q1 (s->A) and Q2 (B->t) with a shortest A->B path
// avoiding them; keep it if it is the best NSP so far.
void try_candidate(const Graph& g, VertexId s, VertexId t,
                   const DijkstraResult& dS, Weight st_dist,
                   VertexId A, VertexId B,
                   const std::vector<EdgeId>& q1_edges,
                   const std::vector<EdgeId>& q2_edges,
                   Weight& best_cost, std::vector<EdgeId>& best_edges) {
    const VertexId n = g.num_vertices();
    std::vector<char> allowed(n, 1);
    // Block Q1 and Q2 vertices; A and B stay allowed.
    if (!q1_edges.empty()) {
        allowed[s] = 0;
        for (EdgeId eid : q1_edges) allowed[g.edge(eid).dst] = 0;
    } else if (A == s) {
        // A == s: nothing to block.
    }
    if (!q2_edges.empty()) {
        allowed[t] = 0;
        for (EdgeId eid : q2_edges) {
            allowed[g.edge(eid).src] = 0;
        }
    }
    allowed[A] = 1;
    allowed[B] = 1;

    auto md = dijkstra_masked(g, A, allowed);
    if (md.dist[B] >= kInfWeight) return;

    Weight cost = dS.dist[A] + md.dist[B] + (st_dist - dS.dist[B]);
    if (cost <= st_dist || cost >= best_cost) return;

    auto p0_edges = recover_path(g, md.parent, A, B);

    std::vector<EdgeId> path;
    path.reserve(q1_edges.size() + p0_edges.size() + q2_edges.size());
    for (EdgeId e : q1_edges) path.push_back(e);
    for (EdgeId e : p0_edges) path.push_back(e);
    for (EdgeId e : q2_edges) path.push_back(e);
    best_cost = cost;
    best_edges = std::move(path);
}

}  // namespace

NspResult cwz_nsp_layered(const Graph& g, VertexId s, VertexId t) {
    NspResult result;
    if (s == t || g.num_vertices() == 0) return result;
    auto dS = dijkstra(g, s);
    auto dT = dijkstra_reverse(g, t);
    if (dS.dist[t] >= kInfWeight) return result;
    result.shortest_cost = dS.dist[t];
    const Weight st_dist = dS.dist[t];
    const VertexId n = g.num_vertices();

    auto fwd = build_forward(g, dS);

    // Endpoints of back-edges.
    std::vector<char> is_back_src(n, 0);
    std::vector<char> is_back_dst(n, 0);
    bool any_back = false;
    for (EdgeId i = 0; i < g.num_edges(); ++i) {
        if (!fwd.is_forward[i]) {
            const Edge& e = g.edge(i);
            if (dS.dist[e.src] < kInfWeight && dT.dist[e.dst] < kInfWeight) {
                is_back_src[e.src] = 1;
                is_back_dst[e.dst] = 1;
                any_back = true;
            }
        }
    }
    if (!any_back) return result;

    Weight best_cost = kInfWeight;
    std::vector<EdgeId> best_edges;

    // Lemma 5.3: for each barrier between layers L and L+1, pick crossing edges
    // (X',X), (Y',Y) and back-edge endpoints A (layer > L), B (layer <= L). Two
    // 2-VDP queries give disjoint P1 = s->X'->X->A and P2 = B->Y'->Y->t.
    std::vector<Weight> distinct_d;
    distinct_d.reserve(n);
    for (VertexId v = 0; v < n; ++v) {
        if (dS.dist[v] < kInfWeight) distinct_d.push_back(dS.dist[v]);
    }
    std::sort(distinct_d.begin(), distinct_d.end());
    distinct_d.erase(std::unique(distinct_d.begin(), distinct_d.end()), distinct_d.end());
    auto layer_of = [&](Weight d) -> int {
        return static_cast<int>(std::lower_bound(distinct_d.begin(), distinct_d.end(), d) -
                                distinct_d.begin());
    };
    // Forward edges crossing each barrier.
    std::vector<std::vector<EdgeId>> cross_at(distinct_d.size());
    for (EdgeId fi = 0; fi < fwd.fg.num_edges(); ++fi) {
        const Edge& e = fwd.fg.edge(fi);
        int Lu = layer_of(dS.dist[e.src]);
        int Lv = layer_of(dS.dist[e.dst]);
        if (Lv == Lu + 1) cross_at[Lu].push_back(fi);
    }

    for (int L = 0; L + 1 < static_cast<int>(distinct_d.size()); ++L) {
        const auto& crosses = cross_at[L];
        if (crosses.size() < 2) continue;
        for (std::size_t i = 0; i < crosses.size(); ++i) {
            VertexId Xprime = fwd.fg.edge(crosses[i]).src;
            VertexId X = fwd.fg.edge(crosses[i]).dst;
            EdgeId eX_orig = fwd.map_to_original[crosses[i]];
            for (std::size_t j = 0; j < crosses.size(); ++j) {
                if (i == j) continue;
                VertexId Yprime = fwd.fg.edge(crosses[j]).src;
                VertexId Y = fwd.fg.edge(crosses[j]).dst;
                if (Xprime == Yprime || X == Y) continue;
                EdgeId eY_orig = fwd.map_to_original[crosses[j]];

                // A == X and B == Y' (trivial halves) are allowed.
                for (VertexId A = 0; A < n; ++A) {
                    if (!is_back_src[A]) continue;
                    if (A == s || A == t) continue;
                    int LA = layer_of(dS.dist[A]);
                    if (LA < L + 1) continue;
                    auto upper = two_vdp_in_dag(fwd.fg, X, A, Y, t);
                    if (!upper) continue;
                    for (VertexId B = 0; B < n; ++B) {
                        if (!is_back_dst[B]) continue;
                        if (B == s || B == t) continue;
                        int LB = layer_of(dS.dist[B]);
                        if (LB > L) continue;
                        auto lower = two_vdp_in_dag(fwd.fg, s, Xprime, B, Yprime);
                        if (!lower) continue;

                        std::vector<EdgeId> q1;
                        q1.reserve(lower->p1.size() + 1 + upper->p1.size());
                        for (EdgeId fid : lower->p1) q1.push_back(fwd.map_to_original[fid]);
                        q1.push_back(eX_orig);
                        for (EdgeId fid : upper->p1) q1.push_back(fwd.map_to_original[fid]);

                        std::vector<EdgeId> q2;
                        q2.reserve(lower->p2.size() + 1 + upper->p2.size());
                        for (EdgeId fid : lower->p2) q2.push_back(fwd.map_to_original[fid]);
                        q2.push_back(eY_orig);
                        for (EdgeId fid : upper->p2) q2.push_back(fwd.map_to_original[fid]);

                        try_candidate(g, s, t, dS, st_dist, A, B, q1, q2,
                                      best_cost, best_edges);
                    }
                }
            }
        }
    }

    if (best_cost < kInfWeight) {
        result.cost = best_cost;
        result.edges = std::move(best_edges);
    }
    return result;
}

NspResult cwz_nsp(const Graph& g, VertexId s, VertexId t) {
    // Straight + layered reductions, then NextSP-Layered; the answer is the
    // cheapest of its result and the reduction candidates.
    NspResult r;
    if (s == t || g.num_vertices() == 0) return r;

    auto reduced = reduce_full(g, s, t);
    if (reduced.s_prime == kNoVertex || reduced.t_prime == kNoVertex) {
        return r;
    }

    auto layered = cwz_nsp_layered(reduced.g_prime, reduced.s_prime, reduced.t_prime);
    r.shortest_cost = layered.shortest_cost;
    if (r.shortest_cost >= kInfWeight) return r;

    Weight best_cost = kInfWeight;
    std::vector<EdgeId> best_edges;

    // Layered result, expanded to original edges.
    if (layered.cost < kInfWeight) {
        best_cost = layered.cost;
        best_edges.clear();
        for (EdgeId reduced_eid : layered.edges) {
            const auto& expansion = reduced.path_expand[reduced_eid];
            best_edges.insert(best_edges.end(), expansion.begin(), expansion.end());
        }
    }

    // Reduction candidates.
    for (const auto& cand : reduced.candidates) {
        if (cand.cost > r.shortest_cost && cand.cost < best_cost) {
            best_cost = cand.cost;
            best_edges = cand.path;
        }
    }

    if (best_cost < kInfWeight) {
        r.cost = best_cost;
        r.edges = std::move(best_edges);
    }
    return r;
}

}  // namespace cwz
