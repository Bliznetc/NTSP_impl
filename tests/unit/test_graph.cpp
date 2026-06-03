#include <gtest/gtest.h>

#include "graph/graph.h"

using namespace cwz;

TEST(Graph, EmptyGraph) {
    Graph g(5);
    EXPECT_EQ(g.num_vertices(), 5);
    EXPECT_EQ(g.num_edges(), 0);
    EXPECT_TRUE(g.out_edges(0).empty());
    EXPECT_TRUE(g.in_edges(4).empty());
}

TEST(Graph, AddEdgeIndexing) {
    Graph g(3);
    EdgeId e0 = g.add_edge(0, 1, 5);
    EdgeId e1 = g.add_edge(1, 2, 7);
    EdgeId e2 = g.add_edge(0, 2, 100);
    EXPECT_EQ(e0, 0);
    EXPECT_EQ(e1, 1);
    EXPECT_EQ(e2, 2);
    EXPECT_EQ(g.num_edges(), 3);

    ASSERT_EQ(g.out_edges(0).size(), 2u);
    EXPECT_EQ(g.edge(g.out_edges(0)[0]).dst, 1);
    EXPECT_EQ(g.edge(g.out_edges(0)[1]).dst, 2);

    ASSERT_EQ(g.in_edges(2).size(), 2u);
    EXPECT_EQ(g.edge(g.in_edges(2)[0]).src, 1);
    EXPECT_EQ(g.edge(g.in_edges(2)[1]).src, 0);
}

TEST(Graph, AddVertexExtends) {
    Graph g(2);
    g.add_edge(0, 1, 3);
    VertexId v = g.add_vertex();
    EXPECT_EQ(v, 2);
    EXPECT_EQ(g.num_vertices(), 3);
    g.add_edge(1, 2, 4);
    EXPECT_EQ(g.num_edges(), 2);
}

TEST(Graph, RejectsNonPositiveWeight) {
    Graph g(2);
    EXPECT_THROW(g.add_edge(0, 1, 0), std::invalid_argument);
    EXPECT_THROW(g.add_edge(0, 1, -1), std::invalid_argument);
}

TEST(Graph, RoundTripText) {
    Graph g(4);
    g.add_edge(0, 1, 2);
    g.add_edge(1, 3, 3);
    g.add_edge(0, 2, 5);
    g.add_edge(2, 3, 1);

    std::string s = g.to_text();
    Graph h = Graph::from_text(s);

    EXPECT_EQ(h.num_vertices(), 4);
    EXPECT_EQ(h.num_edges(), 4);
    EXPECT_EQ(h.edge(0).src, 0);
    EXPECT_EQ(h.edge(0).dst, 1);
    EXPECT_EQ(h.edge(0).w, 2);
    EXPECT_EQ(h.edge(3).src, 2);
    EXPECT_EQ(h.edge(3).dst, 3);
}
