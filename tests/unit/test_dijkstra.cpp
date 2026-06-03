#include <gtest/gtest.h>

#include "graph/graph.h"
#include "shortest_path/dijkstra.h"

using namespace cwz;

TEST(Dijkstra, SingleVertex) {
    Graph g(1);
    auto r = dijkstra(g, 0);
    EXPECT_EQ(r.dist[0], 0);
    EXPECT_EQ(r.parent[0], kNoEdge);
}

TEST(Dijkstra, UnreachableVertexHasInfDistance) {
    Graph g(3);
    g.add_edge(0, 1, 5);
    // vertex 2 isolated
    auto r = dijkstra(g, 0);
    EXPECT_EQ(r.dist[0], 0);
    EXPECT_EQ(r.dist[1], 5);
    EXPECT_EQ(r.dist[2], kInfWeight);
}

TEST(Dijkstra, ChoosesShorterOfTwoPaths) {
    // 0 -10-> 1 -10-> 2
    // 0 ----> 3 ----> 2  (weights 4, 4 = total 8 < 20)
    Graph g(4);
    g.add_edge(0, 1, 10);
    g.add_edge(1, 2, 10);
    g.add_edge(0, 3, 4);
    g.add_edge(3, 2, 4);
    auto r = dijkstra(g, 0);
    EXPECT_EQ(r.dist[2], 8);
}

TEST(Dijkstra, DirectionalityMattersForDirectedGraph) {
    // 0 -> 1 only; reverse should not propagate backward
    Graph g(2);
    g.add_edge(0, 1, 7);
    auto fwd = dijkstra(g, 0);
    EXPECT_EQ(fwd.dist[1], 7);
    auto from1 = dijkstra(g, 1);
    EXPECT_EQ(from1.dist[0], kInfWeight);
}

TEST(DijkstraReverse, ComputesDistanceToTarget) {
    // 0 -> 1 -> 2; distances to 2: from 0 = 5+3 = 8, from 1 = 3, from 2 = 0
    Graph g(3);
    g.add_edge(0, 1, 5);
    g.add_edge(1, 2, 3);
    auto r = dijkstra_reverse(g, 2);
    EXPECT_EQ(r.dist[2], 0);
    EXPECT_EQ(r.dist[1], 3);
    EXPECT_EQ(r.dist[0], 8);
}

TEST(DijkstraReverse, MatchesForwardOnTransposeShape) {
    // Asymmetric distances: from-s and to-t should differ when graph is asymmetric.
    Graph g(3);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 1);
    g.add_edge(0, 2, 100);
    auto fwd = dijkstra(g, 0);
    auto rev = dijkstra_reverse(g, 2);
    EXPECT_EQ(fwd.dist[2], 2);
    EXPECT_EQ(rev.dist[0], 2);
    EXPECT_EQ(rev.dist[1], 1);
}
