#include <gtest/gtest.h>

#include <random>

#include "baseline/brute_force.h"
#include "generators/generators.h"
#include "graph/graph.h"

using namespace cwz;

// Path is simple, s->t, and its weight equals the reported cost.
::testing::AssertionResult check_nsp_path(
    const Graph& g, VertexId s, VertexId t,
    Weight reported_cost, const std::vector<EdgeId>& path) {
    if (path.empty())
        return ::testing::AssertionFailure() << "path is empty";
    std::vector<char> seen(g.num_vertices(), 0);
    seen[s] = 1;
    VertexId cur = s;
    Weight total = 0;
    for (EdgeId eid : path) {
        const Edge& e = g.edge(eid);
        if (e.src != cur)
            return ::testing::AssertionFailure()
                << "edge " << eid << ": src=" << e.src << " but current vertex is " << cur;
        cur = e.dst;
        if (seen[cur])
            return ::testing::AssertionFailure() << "path revisits vertex " << cur;
        seen[cur] = 1;
        total += e.w;
    }
    if (cur != t)
        return ::testing::AssertionFailure()
            << "path ends at " << cur << " but should end at " << t;
    if (total != reported_cost)
        return ::testing::AssertionFailure()
            << "sum of edge weights " << total
            << " disagrees with reported cost " << reported_cost;
    return ::testing::AssertionSuccess();
}

TEST(BruteForceNsp, SingleEdgeNoNsp) {
    Graph g(2);
    g.add_edge(0, 1, 5);
    auto r = brute_force_nsp(g, 0, 1);
    EXPECT_EQ(r.shortest_cost, 5);
    EXPECT_EQ(r.cost, kInfWeight);
    EXPECT_TRUE(r.edges.empty());
}

TEST(BruteForceNsp, TwoParallelPaths) {
    // Shortest 0->1->3 (cost 2), NSP 0->2->3 (cost 3).
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(0, 2, 2);
    g.add_edge(2, 3, 1);
    auto r = brute_force_nsp(g, 0, 3);
    EXPECT_EQ(r.shortest_cost, 2);
    EXPECT_EQ(r.cost, 3);
    EXPECT_EQ(r.edges.size(), 2u);
}

TEST(BruteForceNsp, MultipleShortestPathsThenNsp) {
    // Two shortest paths of cost 2, NSP must be strictly longer.
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(0, 2, 1);
    g.add_edge(2, 3, 1);
    g.add_edge(0, 3, 5);  // direct, longer
    auto r = brute_force_nsp(g, 0, 3);
    EXPECT_EQ(r.shortest_cost, 2);
    EXPECT_EQ(r.cost, 5);
    EXPECT_EQ(r.edges.size(), 1u);
}

TEST(BruteForceNsp, NoNspWhenOnlyOnePathExists) {
    Graph g(3);
    g.add_edge(0, 1, 4);
    g.add_edge(1, 2, 5);
    auto r = brute_force_nsp(g, 0, 2);
    EXPECT_EQ(r.shortest_cost, 9);
    EXPECT_EQ(r.cost, kInfWeight);
}

TEST(BruteForceNsp, EnforcesVertexCap) {
    Graph g(40);
    EXPECT_THROW(brute_force_nsp(g, 0, 39), std::length_error);
}

TEST(BruteForceNsp, MediumHandBuiltGraph) {
    // Two cost-5 paths, NSP 0->1->2->8->9 (cost 6), direct 0->9 (cost 11).
    Graph g(10);
    g.add_edge(0, 1, 1);  g.add_edge(1, 2, 1);
    g.add_edge(2, 3, 1);  g.add_edge(3, 4, 1);
    g.add_edge(4, 9, 1);
    g.add_edge(0, 5, 1);  g.add_edge(5, 6, 1);
    g.add_edge(6, 7, 1);  g.add_edge(7, 8, 1);
    g.add_edge(8, 9, 1);
    g.add_edge(2, 8, 3);
    g.add_edge(0, 9, 11);

    auto r = brute_force_nsp(g, 0, 9);
    EXPECT_EQ(r.shortest_cost, 5);
    EXPECT_EQ(r.cost, 6);
    EXPECT_TRUE(check_nsp_path(g, 0, 9, r.cost, r.edges));
}

TEST(BruteForceNsp, DiamondChainK5) {
    // 32 shortest paths of cost 10; the NSP is the direct edge (cost 11).
    Graph g = gen::diamond_chain(/*k=*/5, /*w=*/1, /*nsp_extra=*/1);
    VertexId t = g.num_vertices() - 1;
    auto r = brute_force_nsp(g, 0, t);
    EXPECT_EQ(r.shortest_cost, 10);
    EXPECT_EQ(r.cost, 11);
    EXPECT_TRUE(check_nsp_path(g, 0, t, r.cost, r.edges));
    // NSP is just the direct shortcut edge.
    EXPECT_EQ(r.edges.size(), 1u);
}

TEST(BruteForceNsp, GeneratedLayeredGraphsHaveValidNspWhenItExists) {
    // Any returned NSP on random layered graphs is a valid, longer path.
    std::mt19937 rng(0x1234);
    int with_nsp = 0;
    for (int trial = 0; trial < 20; ++trial) {
        Graph g = gen::layered(/*layers=*/4, /*width=*/2, /*p_forward=*/0.6,
                               /*p_back=*/0.2, /*max_weight=*/5, rng);
        VertexId t = g.num_vertices() - 1;
        auto r = brute_force_nsp(g, 0, t);
        if (r.shortest_cost >= kInfWeight) continue;   // s, t disconnected
        if (r.cost >= kInfWeight) continue;            // no NSP exists
        ++with_nsp;
        EXPECT_GT(r.cost, r.shortest_cost) << "NSP must be strictly longer";
        EXPECT_TRUE(check_nsp_path(g, 0, t, r.cost, r.edges));
    }
    // The generator must produce some NSPs.
    EXPECT_GT(with_nsp, 0);
}
