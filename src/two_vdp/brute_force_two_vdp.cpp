#include "two_vdp/brute_force_two_vdp.h"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace cwz {

namespace {

// Appends the vertex set of every simple u -> t path that extends `cur`.
void enum_paths(const Graph& g, VertexId u, VertexId t, VertexMask cur,
                std::vector<VertexMask>& out) {
    if (u == t) {
        out.push_back(cur);
        return;
    }
    for (EdgeId e : g.out_edges(u)) {
        VertexId w = g.edge(e).dst;
        if ((cur >> w) & 1) continue;
        enum_paths(g, w, t, cur | (VertexMask{1} << w), out);
    }
}

void check_size(const Graph& g, const char* who) {
    if (g.num_vertices() > 64)
        throw std::length_error(std::string(who) + ": at most 64 vertices supported");
}

void check_vertex(const Graph& g, VertexId v, const char* who) {
    if (v < 0 || v >= g.num_vertices())
        throw std::out_of_range(std::string(who) + ": vertex out of range");
}

}  // namespace

std::vector<VertexMask> brute_force_path_vertex_sets(const Graph& g, VertexId s,
                                                     VertexId t) {
    const char* who = "brute_force_path_vertex_sets";
    check_size(g, who);
    check_vertex(g, s, who);
    check_vertex(g, t, who);
    std::vector<VertexMask> out;
    enum_paths(g, s, t, VertexMask{1} << s, out);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

bool any_disjoint_vertex_sets(const std::vector<VertexMask>& a,
                              const std::vector<VertexMask>& b) {
    for (VertexMask x : a)
        for (VertexMask y : b)
            if ((x & y) == 0) return true;
    return false;
}

bool brute_force_two_vdp_feasible(const Graph& g, VertexId s1, VertexId t1,
                                  VertexId s2, VertexId t2) {
    if (s1 == s2 || t1 == t2 || s1 == t2 || s2 == t1) return false;
    return any_disjoint_vertex_sets(brute_force_path_vertex_sets(g, s1, t1),
                                    brute_force_path_vertex_sets(g, s2, t2));
}

}  // namespace cwz
