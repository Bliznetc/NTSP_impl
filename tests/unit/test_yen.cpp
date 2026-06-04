#include <gtest/gtest.h>

#include <cmath>
#include <random>

#include "baseline/brute_force.h"
#include "baseline/yen.h"
#include "generators/generators.h"
#include "graph/graph.h"

using namespace cwz;

namespace {

// Validate that a returned NSP path: starts at s, ends at t, edges chain,
// no vertex revisited, sum of edge weights equals the reported cost.
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
                << "edge " << eid << ": src=" << e.src << " but current is " << cur;
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

}  // namespace

TEST(YenNsp, FindsSecondPath) {
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(0, 2, 2);
    g.add_edge(2, 3, 1);
    auto r = yen_nsp(g, 0, 3);
    EXPECT_EQ(r.shortest_cost, 2);
    EXPECT_EQ(r.cost, 3);
}

TEST(YenNsp, NoNsp) {
    Graph g(2);
    g.add_edge(0, 1, 7);
    auto r = yen_nsp(g, 0, 1);
    EXPECT_EQ(r.shortest_cost, 7);
    EXPECT_EQ(r.cost, kInfWeight);
}

TEST(YenNsp, SkipsAlternateShortestPaths) {
    // 0->1->3 and 0->2->3 both cost 2; NSP is the direct 0->3 (cost 5).
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(0, 2, 1);
    g.add_edge(2, 3, 1);
    g.add_edge(0, 3, 5);
    auto r = yen_nsp(g, 0, 3);
    EXPECT_EQ(r.shortest_cost, 2);
    EXPECT_EQ(r.cost, 5);
}

TEST(YenNsp, MediumHandBuiltGraph) {
    // 10 vertices. Two parallel forward paths of cost 5 plus a (2,8,3)
    // shortcut and a direct (0,9,11) back-edge. Simple s->t paths:
    //   0->1->2->3->4->9      cost 5  (shortest)
    //   0->5->6->7->8->9      cost 5  (alternate shortest)
    //   0->1->2->8->9         cost 6  (NSP via the (2,8) shortcut)
    //   0->9                  cost 11
    //
    // Yen must (a) iterate past the alternate cost-5 path, (b) return the
    // cost-6 path before considering the cost-11 direct edge. Exercises the
    // "keep going past equal-cost alternates" path through line 152 of yen.cpp.
    Graph g(10);
    g.add_edge(0, 1, 1);  g.add_edge(1, 2, 1);
    g.add_edge(2, 3, 1);  g.add_edge(3, 4, 1);
    g.add_edge(4, 9, 1);
    g.add_edge(0, 5, 1);  g.add_edge(5, 6, 1);
    g.add_edge(6, 7, 1);  g.add_edge(7, 8, 1);
    g.add_edge(8, 9, 1);
    g.add_edge(2, 8, 3);
    g.add_edge(0, 9, 11);

    auto r = yen_nsp(g, 0, 9);
    EXPECT_EQ(r.shortest_cost, 5);
    EXPECT_EQ(r.cost, 6);
    EXPECT_TRUE(check_nsp_path(g, 0, 9, r.cost, r.edges));
}

TEST(YenNsp, DiamondChainK4ExtensiveAlternates) {
    // 2^4 = 16 shortest s->t paths of cost 8, plus a direct shortcut of cost 9.
    // Yen must iterate past all 16 before promoting the shortcut to A and
    // recognizing it as the NSP. Stresses both the spur loop and the
    // dedup `seen` set: many different (root, spur) decompositions of the
    // same shortest path will be generated and must collapse to one entry.
    Graph g = gen::diamond_chain(/*k=*/4, /*w=*/1, /*nsp_extra=*/1);
    VertexId t = g.num_vertices() - 1;
    auto r = yen_nsp(g, 0, t);
    EXPECT_EQ(r.shortest_cost, 8);
    EXPECT_EQ(r.cost, 9);
    EXPECT_TRUE(check_nsp_path(g, 0, t, r.cost, r.edges));
    EXPECT_EQ(r.edges.size(), 1u);  // NSP is just the direct shortcut edge
}

TEST(YenNsp, KMaxCapReturnsNoNspGracefully) {
    // If we cap k_max far below the number of distinct shortest paths,
    // Yen never reaches the strictly-longer candidate and must return
    // {cost=kInfWeight, edges={}} rather than fabricating a wrong answer.
    // Uses diamond_chain k=4 (16 shortest paths) but with k_max=3.
    Graph g = gen::diamond_chain(/*k=*/4, /*w=*/1, /*nsp_extra=*/1);
    VertexId t = g.num_vertices() - 1;
    auto r = yen_nsp(g, 0, t, /*k_max=*/3);
    EXPECT_EQ(r.shortest_cost, 8);
    EXPECT_EQ(r.cost, kInfWeight) << "k_max cap should produce a false negative, not a wrong NSP";
    EXPECT_TRUE(r.edges.empty());
}

TEST(YenNsp, MatchesBruteForceOnGeneratedGraphs) {
    // Cross-check Yen against the brute-force oracle on 25 seeded random
    // graphs across all generator families. They must agree on both
    // shortest_cost and nsp_cost (the only "wiggle room" Yen has is which
    // of several equal-cost paths it returns; brute force can disagree on
    // the *path* but not the *cost*). Whenever Yen returns an NSP, validate
    // its structural correctness.
    std::mt19937 rng(0xBEEF);
    int verified_nsp = 0;
    for (int trial = 0; trial < 25; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int n = std::uniform_int_distribution<int>(4, 8)(rng);
        Graph g(0);
        switch (kind) {
            case 0: g = gen::erdos_renyi(n, 0.40, 9, rng); break;
            case 1: g = gen::random_dag(n, 0.45, 9, rng); break;
            case 2: g = gen::grid(std::max(2, (int)std::sqrt((double)n)),
                                  std::max(2, (int)std::sqrt((double)n)), 9, rng); break;
            case 3: g = gen::layered(std::max(2, n / 3), std::max(1, (n - 2) / 3),
                                     0.5, 0.15, 9, rng); break;
        }
        if (g.num_vertices() < 2) continue;
        VertexId t = g.num_vertices() - 1;
        auto r_yen = yen_nsp(g, 0, t);
        auto r_bf = brute_force_nsp(g, 0, t);
        EXPECT_EQ(r_yen.shortest_cost, r_bf.shortest_cost)
            << "trial " << trial << " kind=" << kind << " n=" << g.num_vertices();
        EXPECT_EQ(r_yen.cost, r_bf.cost)
            << "trial " << trial << " kind=" << kind << " n=" << g.num_vertices();
        if (r_yen.cost < kInfWeight) {
            EXPECT_TRUE(check_nsp_path(g, 0, t, r_yen.cost, r_yen.edges))
                << "trial " << trial;
            ++verified_nsp;
        }
    }
    EXPECT_GT(verified_nsp, 0)
        << "no trial produced an NSP; generator parameters may be too sparse";
}
