#include <gtest/gtest.h>

#include "generators/generators.h"

using namespace cwz;

TEST(Generators, ErdosRenyiHasPositiveWeightsAndNoSelfLoops) {
    std::mt19937 rng(42);
    Graph g = gen::erdos_renyi(20, 0.3, 10, rng);
    EXPECT_EQ(g.num_vertices(), 20);
    for (EdgeId i = 0; i < g.num_edges(); ++i) {
        const auto& e = g.edge(i);
        EXPECT_NE(e.src, e.dst);
        EXPECT_GT(e.w, 0);
        EXPECT_LE(e.w, 10);
    }
}

TEST(Generators, RandomDagIsAcyclic) {
    std::mt19937 rng(7);
    Graph g = gen::random_dag(15, 0.5, 100, rng);
    for (EdgeId i = 0; i < g.num_edges(); ++i) {
        EXPECT_LT(g.edge(i).src, g.edge(i).dst);
    }
}

TEST(Generators, LayeredHasExpectedSize) {
    std::mt19937 rng(1);
    Graph g = gen::layered(4, 3, 0.6, 0.0, 5, rng);
    // 2 endpoints + 4 layers * 3 = 14 vertices
    EXPECT_EQ(g.num_vertices(), 14);
}

TEST(Generators, GridHasCorrectVertexCount) {
    std::mt19937 rng(2);
    Graph g = gen::grid(3, 4, 7, rng);
    EXPECT_EQ(g.num_vertices(), 12);
    // edges = (rows-1)*cols + rows*(cols-1) = 2*4 + 3*3 = 17
    EXPECT_EQ(g.num_edges(), 17);
}

TEST(Generators, DiamondChainCorrectShape) {
    Graph g = gen::diamond_chain(3, 1, 1);
    // 1 + 3*3 = 10 vertices; 4*3 + 1 = 13 edges
    EXPECT_EQ(g.num_vertices(), 10);
    EXPECT_EQ(g.num_edges(), 13);
    // Last edge is the s -> t shortcut with weight 2*3*1 + 1 = 7
    const Edge& shortcut = g.edge(g.num_edges() - 1);
    EXPECT_EQ(shortcut.src, 0);
    EXPECT_EQ(shortcut.dst, 9);
    EXPECT_EQ(shortcut.w, 7);
}

TEST(Generators, ScaleFreeReturnsGraph) {
    std::mt19937 rng(3);
    Graph g = gen::scale_free(20, 2, 5, rng);
    EXPECT_EQ(g.num_vertices(), 20);
    EXPECT_GT(g.num_edges(), 0);
}
