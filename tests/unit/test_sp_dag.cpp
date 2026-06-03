#include <gtest/gtest.h>

#include <algorithm>

#include "graph/graph.h"
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

TEST(SpDag, TargetUnreachable) {
    Graph g(3);
    g.add_edge(0, 1, 1);
    // 2 has no incoming edges
    auto dag = build_sp_dag(g, 0, 2);
    EXPECT_EQ(dag.st_distance, kInfWeight);
    EXPECT_TRUE(dag.edges_on_sp.empty());
    EXPECT_TRUE(dag.vertex_on_sp.empty());
}
