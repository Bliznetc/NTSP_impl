#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <set>

#include "generators/generators.h"
#include "graph/graph.h"
#include "shortest_path/dijkstra.h"
#include "shortest_path/sp_dag.h"

using namespace cwz;

namespace {

bool has_edge(const ShortestPathDag& dag, EdgeId e) {
    return std::find(dag.edges_on_sp.begin(), dag.edges_on_sp.end(), e) !=
           dag.edges_on_sp.end();
}

}  // namespace

TEST(SpDag, SingleShortestPath) {
    // 0 -1-> 1 -1-> 2, plus 0 -100-> 2 (not on shortest path).
    Graph g(3);
    EdgeId e01 = g.add_edge(0, 1, 1);
    EdgeId e12 = g.add_edge(1, 2, 1);
    EdgeId e02 = g.add_edge(0, 2, 100);
    auto dag = build_sp_dag(g, 0, 2);
    EXPECT_EQ(dag.st_distance, 2);
    EXPECT_EQ(dag.edges_on_sp.size(), 2u);
    EXPECT_TRUE(has_edge(dag, e01));
    EXPECT_TRUE(has_edge(dag, e12));
    EXPECT_FALSE(has_edge(dag, e02));
    EXPECT_TRUE(dag.vertex_on_sp[0]);
    EXPECT_TRUE(dag.vertex_on_sp[1]);
    EXPECT_TRUE(dag.vertex_on_sp[2]);
}

TEST(SpDag, MultipleShortestPathsAllIncluded) {
    // Two paths of length 2: 0->1->3 and 0->2->3.
    Graph g(4);
    EdgeId e01 = g.add_edge(0, 1, 1);
    EdgeId e13 = g.add_edge(1, 3, 1);
    EdgeId e02 = g.add_edge(0, 2, 1);
    EdgeId e23 = g.add_edge(2, 3, 1);
    auto dag = build_sp_dag(g, 0, 3);
    EXPECT_EQ(dag.st_distance, 2);
    EXPECT_EQ(dag.edges_on_sp.size(), 4u);
    EXPECT_TRUE(has_edge(dag, e01));
    EXPECT_TRUE(has_edge(dag, e13));
    EXPECT_TRUE(has_edge(dag, e02));
    EXPECT_TRUE(has_edge(dag, e23));
    for (int v = 0; v < 4; ++v) EXPECT_TRUE(dag.vertex_on_sp[v]);
}

TEST(SpDag, VertexOffShortestPathExcluded) {
    // 0 -> 1 -> 2 (shortest, length 2), and a detour 0 -> 3 -> 2 with weight 5
    // means 3 is not on any shortest path.
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 1);
    g.add_edge(0, 3, 2);
    g.add_edge(3, 2, 3);
    auto dag = build_sp_dag(g, 0, 2);
    EXPECT_EQ(dag.st_distance, 2);
    EXPECT_TRUE(dag.vertex_on_sp[0]);
    EXPECT_TRUE(dag.vertex_on_sp[1]);
    EXPECT_TRUE(dag.vertex_on_sp[2]);
    EXPECT_FALSE(dag.vertex_on_sp[3]);
    EXPECT_EQ(dag.edges_on_sp.size(), 2u);
}

TEST(SpDag, LargerGraphWithMixedOnAndOffSpStructure) {
    // 8-vertex graph. Four distinct shortest 0->7 paths each of weight 4,
    // plus an off-SP detour through vertex 6 of weight 20.
    //
    //   0 ---1--> 1 ---1--> 3 ---1--> 4 ---1--> 7
    //    \         \         \         /
    //   1 \         \         \-->5--/-1
    //      \         \                ^
    //       v         v               |
    //       2 ---1--> 3 (merge)       |
    //
    //   0 --10--> 6 --10--> 7      (off-SP detour)
    //
    // Distances from 0:
    //   dS = [0, 1, 1, 2, 3, 3, 10, 4]
    // Distances to 7:
    //   dT = [4, 3, 3, 2, 1, 1, 10, 0]
    // dG(0,7) = 4. Vertex 6 is off SP (dS+dT = 20).
    // 8 edges are on SP; 2 (0->6, 6->7) are not.
    Graph g(8);
    EdgeId e01 = g.add_edge(0, 1, 1);
    EdgeId e02 = g.add_edge(0, 2, 1);
    EdgeId e13 = g.add_edge(1, 3, 1);
    EdgeId e23 = g.add_edge(2, 3, 1);
    EdgeId e34 = g.add_edge(3, 4, 1);
    EdgeId e35 = g.add_edge(3, 5, 1);
    EdgeId e47 = g.add_edge(4, 7, 1);
    EdgeId e57 = g.add_edge(5, 7, 1);
    EdgeId e06 = g.add_edge(0, 6, 10);
    EdgeId e67 = g.add_edge(6, 7, 10);

    auto dag = build_sp_dag(g, 0, 7);
    EXPECT_EQ(dag.st_distance, 4);

    // Vertices 0..5 and 7 are on SP; 6 is not.
    for (int v : {0, 1, 2, 3, 4, 5, 7}) EXPECT_TRUE(dag.vertex_on_sp[v]) << "v=" << v;
    EXPECT_FALSE(dag.vertex_on_sp[6]);

    // SP-DAG edges: the 8 unit-weight edges through the diamond, but not
    // the two off-SP detour edges.
    std::set<EdgeId> sp(dag.edges_on_sp.begin(), dag.edges_on_sp.end());
    EXPECT_EQ(sp.size(), 8u);
    for (EdgeId e : {e01, e02, e13, e23, e34, e35, e47, e57})
        EXPECT_TRUE(sp.count(e)) << "missing edge " << e;
    EXPECT_FALSE(sp.count(e06));
    EXPECT_FALSE(sp.count(e67));
}

TEST(SpDag, ConsistentWithIndependentDijkstrasOnGeneratedGraphs) {
    // Property check: build_sp_dag's claim must match what an independent
    // (dS, dT) computation derives. Specifically, for every random graph:
    //   * st_distance == dS[t]
    //   * vertex_on_sp[v] iff dS[v] + dT[v] == st_distance
    //   * edges_on_sp = { e : dS[e.src] + e.w + dT[e.dst] == st_distance }
    // If the implementation ever drifts between its vertex pass and its
    // edge pass (e.g., re-uses a stale local), this catches it.
    std::mt19937 rng(0x5DEDA6);
    int verified = 0;
    for (int trial = 0; trial < 30; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int n = std::uniform_int_distribution<int>(4, 15)(rng);
        Graph g(0);
        switch (kind) {
            case 0: g = gen::erdos_renyi(n, 0.35, 9, rng); break;
            case 1: g = gen::random_dag(n, 0.40, 9, rng); break;
            case 2: g = gen::grid(std::max(2, (int)std::sqrt((double)n)),
                                  std::max(2, (int)std::sqrt((double)n)), 9, rng); break;
            case 3: g = gen::layered(std::max(2, n / 3), std::max(1, (n - 2) / 3),
                                     0.5, 0.15, 9, rng); break;
        }
        if (g.num_vertices() < 2) continue;
        VertexId t = g.num_vertices() - 1;
        auto dag = build_sp_dag(g, 0, t);
        if (dag.st_distance >= kInfWeight) continue;  // t unreachable, separately tested

        auto dS = dijkstra(g, 0);
        auto dT = dijkstra_reverse(g, t);
        ASSERT_EQ(dag.st_distance, dS.dist[t]);

        // Vertex membership: dag.vertex_on_sp matches the independent formula.
        for (VertexId v = 0; v < g.num_vertices(); ++v) {
            bool expected = dS.dist[v] < kInfWeight && dT.dist[v] < kInfWeight &&
                            dS.dist[v] + dT.dist[v] == dag.st_distance;
            EXPECT_EQ(static_cast<bool>(dag.vertex_on_sp[v]), expected)
                << "trial " << trial << " v=" << v;
        }

        // Edge membership: dag.edges_on_sp is exactly the right set.
        std::set<EdgeId> dag_edges(dag.edges_on_sp.begin(), dag.edges_on_sp.end());
        for (EdgeId i = 0; i < g.num_edges(); ++i) {
            const Edge& e = g.edge(i);
            bool expected = dS.dist[e.src] < kInfWeight && dT.dist[e.dst] < kInfWeight &&
                            dS.dist[e.src] + e.w + dT.dist[e.dst] == dag.st_distance;
            bool in_dag = dag_edges.count(i) > 0;
            EXPECT_EQ(in_dag, expected) << "trial " << trial << " edge " << i;
        }

        // Cross-consistency: every edge in the DAG should have on-SP endpoints.
        for (EdgeId i : dag.edges_on_sp) {
            const Edge& e = g.edge(i);
            EXPECT_TRUE(dag.vertex_on_sp[e.src]) << "trial " << trial << " edge " << i;
            EXPECT_TRUE(dag.vertex_on_sp[e.dst]) << "trial " << trial << " edge " << i;
        }
        ++verified;
    }
    EXPECT_GT(verified, 15) << "too few trials had a reachable t";
}

TEST(SpDag, TargetUnreachable) {
    Graph g(3);
    g.add_edge(0, 1, 1);
    // 2 has no incoming edges
    auto dag = build_sp_dag(g, 0, 2);
    EXPECT_EQ(dag.st_distance, kInfWeight);
    EXPECT_TRUE(dag.edges_on_sp.empty());
    EXPECT_TRUE(dag.vertex_on_sp.empty());
}
