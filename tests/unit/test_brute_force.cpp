#include <gtest/gtest.h>

#include <random>

#include "baseline/brute_force.h"
#include "generators/generators.h"
#include "graph/graph.h"

using namespace cwz;

// Validate a returned NSP path: starts at s, ends at t, edges chain, no vertex
// revisited, and sum of edge weights equals the reported NSP cost.
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
    // 10 vertices, two parallel forward paths of cost 5 plus a (2,8) shortcut
    // and a direct (0,9) edge of weight 11. Simple s->t paths:
    //   0->1->2->3->4->9      cost 5   (shortest)
    //   0->5->6->7->8->9      cost 5   (alternate shortest)
    //   0->1->2->8->9         cost 6   (NSP via the (2,8,3) shortcut)
    //   0->9                  cost 11  (direct back-edge)
    // NSP cost = 6.
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
    // 16 vertices, 21 edges, exactly 2^5 = 32 shortest s->t paths each of
    // weight 10, plus the unique NSP shortcut s->t of weight 11. Stresses the
    // DFS with branch-and-bound pruning: many short paths to ignore before the
    // single NSP candidate.
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
    // Stress test across 20 seeded layered graphs near the cap. Doesn't pin
    // down a specific cost (it depends on the seed) but verifies the
    // structural invariants of whatever NSP brute force returns: simple s->t
    // path, weight matches the reported cost, strictly greater than shortest.
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
    // Sanity: at least some of the 20 trials should yield an NSP. Otherwise
    // the generator parameters are too sparse and this test isn't exercising
    // the NSP path.
    EXPECT_GT(with_nsp, 0);
}
