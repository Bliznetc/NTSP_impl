#include "cwz/reductions.h"

#include <algorithm>
#include <map>
#include <unordered_map>

#include "shortest_path/dijkstra.h"

namespace cwz {

ReductionResult reduce_to_straight(const Graph& g, VertexId s, VertexId t) {
    // Iterative folding of off-SP vertices.
    //
    // Given the (dS, dT) classification, every vertex v with
    // dS[v] + dT[v] > dG(s,t) is "off SP" and contributes only to non-shortest
    // paths through it. We fold these vertices one at a time, replacing the
    // folded vertex u with shortcut edges (x, y) for every (x, u) in-edge and
    // (u, y) out-edge, of weight w(x,u) + w(u,y). The shortcut carries
    // provenance: a concatenated list of the original edges it represents.
    //
    // Iterating handles multi-vertex off-SP chains correctly: if x -> u1 -> u2
    // -> y exists in the original (u1 and u2 both off SP), folding u1 first
    // creates the shortcut x -> u2 of weight w(x,u1)+w(u1,u2); then folding
    // u2 creates the shortcut x -> y of weight w(x,u1)+w(u1,u2)+w(u2,y), with
    // provenance being the three original edges in order.
    //
    // Multi-edges are kept by design: two structurally-distinct off-SP paths
    // between the same on-SP endpoints produce two parallel edges in the
    // reduced graph. The NSP algorithm running downstream needs both, because
    // the cheaper one may be on the shortest path while the costlier is the
    // NSP candidate.
    const VertexId n = g.num_vertices();
    auto dS = dijkstra(g, s);
    auto dT = dijkstra_reverse(g, t);
    const Weight dst = dS.dist[t];

    ReductionResult r;
    r.vertex_old_to_new.assign(n, kNoVertex);

    if (dst >= kInfWeight) {
        // t unreachable; preserve s and t as separate vertices, drop everything else.
        VertexId new_n = 0;
        r.vertex_old_to_new[s] = new_n++;
        if (t != s) r.vertex_old_to_new[t] = new_n++;
        r.g_prime = Graph(new_n);
        r.s_prime = r.vertex_old_to_new[s];
        r.t_prime = r.vertex_old_to_new[t];
        return r;
    }

    std::vector<char> on_sp(n, 0);
    for (VertexId v = 0; v < n; ++v) {
        if (dS.dist[v] < kInfWeight && dT.dist[v] < kInfWeight &&
            dS.dist[v] + dT.dist[v] == dst) {
            on_sp[v] = 1;
        }
    }

    // Mutable edge list. Each entry tracks (src, dst, weight, provenance).
    struct WorkEdge {
        VertexId src;
        VertexId dst;
        Weight w;
        std::vector<EdgeId> provenance;  // ordered original-graph edge IDs this represents
    };
    std::vector<WorkEdge> edges;
    edges.reserve(g.num_edges());
    for (EdgeId i = 0; i < g.num_edges(); ++i) {
        const Edge& e = g.edge(i);
        edges.push_back({e.src, e.dst, e.w, {i}});
    }

    // Fold each off-SP vertex in turn. Iteration order over u doesn't affect
    // the final reduced graph (shown by induction on chain length); we use
    // the natural vertex order.
    for (VertexId u = 0; u < n; ++u) {
        if (on_sp[u]) continue;

        // Partition the current edge list into "incident to u" and "the rest".
        std::vector<WorkEdge> in_edges_of_u;
        std::vector<WorkEdge> out_edges_of_u;
        std::vector<WorkEdge> survivors;
        survivors.reserve(edges.size());
        for (auto& e : edges) {
            if (e.src == u && e.dst == u) {
                // self-loop on u; drop entirely
                continue;
            }
            if (e.dst == u) {
                in_edges_of_u.push_back(std::move(e));
            } else if (e.src == u) {
                out_edges_of_u.push_back(std::move(e));
            } else {
                survivors.push_back(std::move(e));
            }
        }
        edges = std::move(survivors);

        // For each (in, out) pair, create a shortcut. Skip pairs that would
        // produce a self-loop (in.src == out.dst), since the corresponding
        // path in G visits in.src twice and isn't simple.
        //
        // Dedup by (src, dst, weight): we only need one representative per
        // distinct cost between each pair of remaining vertices, because the
        // NSP only depends on the *set* of achievable (src, dst, cost)
        // triples. Without this dedup, dense graphs can generate
        // combinatorially-many redundant multi-edges (Cartesian product over
        // each fold step), driving the reduced graph's edge count to the
        // hundreds of thousands and making the downstream layered algorithm
        // intractable.
        for (const auto& in_e : in_edges_of_u) {
            for (const auto& out_e : out_edges_of_u) {
                if (in_e.src == out_e.dst) continue;
                WorkEdge sc;
                sc.src = in_e.src;
                sc.dst = out_e.dst;
                sc.w = in_e.w + out_e.w;
                sc.provenance.reserve(in_e.provenance.size() + out_e.provenance.size());
                sc.provenance.insert(sc.provenance.end(),
                                     in_e.provenance.begin(), in_e.provenance.end());
                sc.provenance.insert(sc.provenance.end(),
                                     out_e.provenance.begin(), out_e.provenance.end());
                edges.push_back(std::move(sc));
            }
        }

        // Dedup: collapse parallel edges with the same (src, dst, weight) to
        // a single representative, and further limit each (src, dst) pair to
        // the K cheapest distinct weights. For NSP correctness only the very
        // cheap ones matter -- the shortest path uses the minimum, the NSP
        // candidate uses the second-minimum, and higher weights almost never
        // matter unless simplicity-driven constraints force us to one. K=5
        // is a heuristic balance.
        {
            std::map<std::pair<VertexId, VertexId>, std::vector<std::size_t>> by_pair;
            for (std::size_t i = 0; i < edges.size(); ++i) {
                by_pair[{edges[i].src, edges[i].dst}].push_back(i);
            }
            std::vector<WorkEdge> deduped;
            const int K = 5;
            for (auto& [pair, indices] : by_pair) {
                std::sort(indices.begin(), indices.end(),
                          [&](std::size_t a, std::size_t b) {
                              return edges[a].w < edges[b].w;
                          });
                Weight last_w = -1;
                int kept = 0;
                for (std::size_t idx : indices) {
                    if (edges[idx].w == last_w) continue;
                    last_w = edges[idx].w;
                    deduped.push_back(std::move(edges[idx]));
                    if (++kept >= K) break;
                }
            }
            edges = std::move(deduped);
        }
    }

    // Materialize the reduced graph. Vertices are the on-SP ones, in order.
    VertexId new_n = 0;
    for (VertexId v = 0; v < n; ++v) {
        if (on_sp[v]) r.vertex_old_to_new[v] = new_n++;
    }
    r.g_prime = Graph(new_n);
    r.s_prime = r.vertex_old_to_new[s];
    r.t_prime = r.vertex_old_to_new[t];

    for (auto& we : edges) {
        VertexId nsrc = r.vertex_old_to_new[we.src];
        VertexId ndst = r.vertex_old_to_new[we.dst];
        if (nsrc == kNoVertex || ndst == kNoVertex) continue;  // defensive
        EdgeId new_eid = r.g_prime.add_edge(nsrc, ndst, we.w);
        if (new_eid >= static_cast<EdgeId>(r.path_expand.size()))
            r.path_expand.resize(new_eid + 1);
        r.path_expand[new_eid] = std::move(we.provenance);
    }
    return r;
}

namespace {

// Recover the shortest forward s->v path (edge list) from a forward Dijkstra
// parent array. parent[x] is the edge into x on a shortest s->x path.
std::vector<EdgeId> recover_s_to(const Graph& g, const std::vector<EdgeId>& parent,
                                 VertexId s, VertexId v) {
    std::vector<EdgeId> path;
    VertexId cur = v;
    while (cur != s) {
        EdgeId e = parent[cur];
        if (e == kNoEdge) return {};
        path.push_back(e);
        cur = g.edge(e).src;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// Recover the shortest forward v->t path (edge list) from a reverse Dijkstra
// parent array. parent[x] is the out-edge of x on a shortest x->t path.
std::vector<EdgeId> recover_to_t(const Graph& g, const std::vector<EdgeId>& parent,
                                 VertexId v, VertexId t) {
    std::vector<EdgeId> path;
    VertexId cur = v;
    while (cur != t) {
        EdgeId e = parent[cur];
        if (e == kNoEdge) return {};
        path.push_back(e);
        cur = g.edge(e).dst;
    }
    return path;
}

}  // namespace

ReductionResult reduce_to_layered(const Graph& g, VertexId s, VertexId t) {
    // Implements the paper's NextSP-Straight (Section 4.2): given an
    // (s,t)-straight graph, produce a strictly (s,t)-layered graph plus the
    // set of candidate paths through removed (forward-or-sideways) back-edges.
    ReductionResult r;
    const VertexId n = g.num_vertices();
    auto dS = dijkstra(g, s);
    auto dT = dijkstra_reverse(g, t);
    const Weight dst = dS.dist[t];

    if (dst >= kInfWeight) {
        // No s->t path; copy unchanged, no candidates.
        r.g_prime = g;
        r.s_prime = s;
        r.t_prime = t;
        r.vertex_old_to_new.resize(n);
        for (VertexId v = 0; v < n; ++v) r.vertex_old_to_new[v] = v;
        r.path_expand.resize(g.num_edges());
        for (EdgeId i = 0; i < g.num_edges(); ++i) r.path_expand[i] = {i};
        return r;
    }

    // Distinct distance values, sorted; layer index = position in this vector.
    std::vector<Weight> distinct_d;
    distinct_d.reserve(n);
    for (VertexId v = 0; v < n; ++v) {
        if (dS.dist[v] < kInfWeight) distinct_d.push_back(dS.dist[v]);
    }
    std::sort(distinct_d.begin(), distinct_d.end());
    distinct_d.erase(std::unique(distinct_d.begin(), distinct_d.end()), distinct_d.end());

    auto layer = [&](Weight d) -> int {
        return static_cast<int>(std::lower_bound(distinct_d.begin(), distinct_d.end(), d) -
                                distinct_d.begin());
    };

    // Build the new graph: copy all original vertices first.
    r.g_prime = Graph(n);
    r.vertex_old_to_new.assign(n, kNoVertex);
    for (VertexId v = 0; v < n; ++v) r.vertex_old_to_new[v] = v;
    r.s_prime = s;
    r.t_prime = t;

    for (EdgeId i = 0; i < g.num_edges(); ++i) {
        const Edge& e = g.edge(i);
        if (dS.dist[e.src] >= kInfWeight || dS.dist[e.dst] >= kInfWeight) continue;
        bool is_forward = (dS.dist[e.src] + e.w == dS.dist[e.dst]);

        if (!is_forward) {
            // Back-edge. If it goes forward-or-sideways in distance
            // (d(u) <= d(v)) it violates the (s,t)-layered conditions: record
            // the candidate Ps->u o (u,v) o Pv->t and DROP the edge. If it goes
            // strictly backward (d(u) > d(v)) it is layered-legal: keep it.
            if (dS.dist[e.src] <= dS.dist[e.dst]) {
                if (dT.dist[e.dst] < kInfWeight) {
                    ReductionCandidate cand;
                    cand.cost = dS.dist[e.src] + e.w + dT.dist[e.dst];
                    auto pre = recover_s_to(g, dS.parent, s, e.src);
                    auto suf = recover_to_t(g, dT.parent, e.dst, t);
                    cand.path.reserve(pre.size() + 1 + suf.size());
                    cand.path.insert(cand.path.end(), pre.begin(), pre.end());
                    cand.path.push_back(i);
                    cand.path.insert(cand.path.end(), suf.begin(), suf.end());
                    r.candidates.push_back(std::move(cand));
                }
                continue;  // drop the edge
            }
            // Strictly-backward back-edge: keep as-is.
            EdgeId new_id = r.g_prime.add_edge(e.src, e.dst, e.w);
            if (static_cast<EdgeId>(r.path_expand.size()) <= new_id)
                r.path_expand.resize(new_id + 1);
            r.path_expand[new_id] = {i};
            continue;
        }

        // Forward edge.
        int lu = layer(dS.dist[e.src]);
        int lv = layer(dS.dist[e.dst]);
        if (lv == lu + 1) {
            EdgeId new_id = r.g_prime.add_edge(e.src, e.dst, e.w);
            if (static_cast<EdgeId>(r.path_expand.size()) <= new_id)
                r.path_expand.resize(new_id + 1);
            r.path_expand[new_id] = {i};
            continue;
        }
        // Subdivide a layer-skipping forward edge: insert lv-lu-1 intermediate
        // vertices, chaining the sub-edges. Only the first sub-edge carries the
        // original edge ID in its provenance; the rest carry an empty list, so
        // that a full traversal of the chain expands to exactly one copy of the
        // original edge.
        VertexId prev = e.src;
        Weight prev_d = dS.dist[e.src];
        bool first_in_chain = true;
        for (int k = lu + 1; k < lv; ++k) {
            VertexId mid = r.g_prime.add_vertex();
            Weight w_step = distinct_d[k] - prev_d;
            EdgeId new_id = r.g_prime.add_edge(prev, mid, w_step);
            if (static_cast<EdgeId>(r.path_expand.size()) <= new_id)
                r.path_expand.resize(new_id + 1);
            r.path_expand[new_id] = first_in_chain ? std::vector<EdgeId>{i}
                                                   : std::vector<EdgeId>{};
            first_in_chain = false;
            prev = mid;
            prev_d = distinct_d[k];
        }
        Weight w_last = dS.dist[e.dst] - prev_d;
        EdgeId new_id = r.g_prime.add_edge(prev, e.dst, w_last);
        if (static_cast<EdgeId>(r.path_expand.size()) <= new_id)
            r.path_expand.resize(new_id + 1);
        r.path_expand[new_id] = first_in_chain ? std::vector<EdgeId>{i}
                                               : std::vector<EdgeId>{};
    }
    return r;
}

ReductionResult reduce_full(const Graph& g, VertexId s, VertexId t) {
    auto r1 = reduce_to_straight(g, s, t);
    auto r2 = reduce_to_layered(r1.g_prime, r1.s_prime, r1.t_prime);
    // Compose mappings.
    ReductionResult out;
    out.g_prime = std::move(r2.g_prime);
    out.s_prime = r2.s_prime;
    out.t_prime = r2.t_prime;
    out.vertex_old_to_new.assign(r1.vertex_old_to_new.size(), kNoVertex);
    for (VertexId v_old = 0; v_old < static_cast<VertexId>(r1.vertex_old_to_new.size()); ++v_old) {
        VertexId v_mid = r1.vertex_old_to_new[v_old];
        if (v_mid == kNoVertex) continue;
        out.vertex_old_to_new[v_old] = r2.vertex_old_to_new[v_mid];
    }
    // Compose path_expand: each new edge in r2 has expand list referring to
    // edges in r1.g_prime; translate to edges in g.
    out.path_expand.resize(r2.path_expand.size());
    for (EdgeId i = 0; i < static_cast<EdgeId>(r2.path_expand.size()); ++i) {
        for (EdgeId mid_eid : r2.path_expand[i]) {
            const auto& deeper = r1.path_expand[mid_eid];
            out.path_expand[i].insert(out.path_expand[i].end(), deeper.begin(), deeper.end());
        }
    }

    // Candidates. r1.candidates (if any) are already in original-graph edge
    // terms. r2.candidates are in r1.g_prime edge terms; translate each through
    // r1.path_expand to original-graph edges.
    out.candidates = std::move(r1.candidates);
    for (const auto& c : r2.candidates) {
        ReductionCandidate oc;
        oc.cost = c.cost;
        for (EdgeId mid_eid : c.path) {
            const auto& deeper = r1.path_expand[mid_eid];
            oc.path.insert(oc.path.end(), deeper.begin(), deeper.end());
        }
        out.candidates.push_back(std::move(oc));
    }
    return out;
}

}  // namespace cwz
