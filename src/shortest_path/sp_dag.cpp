#include "shortest_path/sp_dag.h"

namespace cwz {

ShortestPathDag build_sp_dag(const Graph& g, VertexId s, VertexId t,
                             const DijkstraResult& from_s,
                             const DijkstraResult& to_t) {
    ShortestPathDag dag;
    const Weight dst = from_s.dist[t];
    dag.st_distance = dst;
    if (dst >= kInfWeight) return dag;

    const VertexId n = g.num_vertices();
    dag.vertex_on_sp.assign(n, 0);
    for (VertexId v = 0; v < n; ++v) {
        if (from_s.dist[v] < kInfWeight && to_t.dist[v] < kInfWeight &&
            from_s.dist[v] + to_t.dist[v] == dst) {
            dag.vertex_on_sp[v] = 1;
        }
    }
    (void)s;  // s is implicit in `from_s`; kept in the signature for clarity
    for (EdgeId i = 0; i < g.num_edges(); ++i) {
        const Edge& e = g.edge(i);
        if (from_s.dist[e.src] >= kInfWeight) continue;
        if (to_t.dist[e.dst] >= kInfWeight) continue;
        if (from_s.dist[e.src] + e.w + to_t.dist[e.dst] == dst) {
            dag.edges_on_sp.push_back(i);
        }
    }
    return dag;
}

}  // namespace cwz
