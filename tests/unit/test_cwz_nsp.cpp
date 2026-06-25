#include <gtest/gtest.h>

#include <cmath>
#include <random>

#include "baseline/brute_force.h"
#include "cwz/nsp.h"
#include "generators/generators.h"
#include "graph/graph.h"

using namespace cwz;

namespace {

// Verify a returned NSP path is structurally valid in g.
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
            << "sum of weights " << total << " disagrees with reported " << reported_cost;
    return ::testing::AssertionSuccess();
}

}  // namespace

TEST(CwzNsp, TwoParallelPaths) {
    // 0->1->3 cost 2 (shortest), 0->2->3 cost 3 (NSP).
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(0, 2, 2);
    g.add_edge(2, 3, 1);
    auto r = cwz_nsp(g, 0, 3);
    EXPECT_EQ(r.shortest_cost, 2);
    EXPECT_EQ(r.cost, 3);
}

TEST(CwzNsp, NoNspWhenAllPathsEqual) {
    // Single path only.
    Graph g(2);
    g.add_edge(0, 1, 7);
    auto r = cwz_nsp(g, 0, 1);
    EXPECT_EQ(r.shortest_cost, 7);
    EXPECT_EQ(r.cost, kInfWeight);
}

TEST(CwzNsp, MatchesBruteForceOnSmallExample) {
    // 6-vertex graph with multiple paths.
    Graph g(6);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 1);
    g.add_edge(2, 5, 1);  // shortest 0->1->2->5 cost 3
    g.add_edge(0, 3, 2);
    g.add_edge(3, 4, 2);
    g.add_edge(4, 5, 2);  // 0->3->4->5 cost 6
    g.add_edge(1, 4, 5);  // shortcut
    auto cwz_r = cwz_nsp(g, 0, 5);
    auto bf = brute_force_nsp(g, 0, 5);
    EXPECT_EQ(cwz_r.shortest_cost, bf.shortest_cost);
    EXPECT_EQ(cwz_r.cost, bf.cost);
}

TEST(CwzNsp, MultiVertexOffSpChainIsFound) {
    // The case that motivated switching to the paper-faithful pipeline.
    // The NSP goes through a chain of three off-SP vertices; only the
    // iterative reduce_to_straight can preserve it.
    Graph g(5);
    g.add_edge(0, 1, 2);
    g.add_edge(1, 2, 2);
    g.add_edge(2, 3, 2);
    g.add_edge(3, 4, 2);
    g.add_edge(0, 4, 3);

    auto cwz_r = cwz_nsp(g, 0, 4);
    auto bf = brute_force_nsp(g, 0, 4);
    EXPECT_EQ(cwz_r.shortest_cost, bf.shortest_cost);
    EXPECT_EQ(cwz_r.shortest_cost, 3);
    EXPECT_EQ(cwz_r.cost, bf.cost);
    EXPECT_EQ(cwz_r.cost, 8);
    EXPECT_TRUE(check_nsp_path(g, 0, 4, cwz_r.cost, cwz_r.edges));
}

TEST(CwzNsp, NspPathReconstructsValidlyInOriginalGraph) {
    // Verify that the path expansion correctly translates back through both
    // reductions to a valid simple path in the original graph.
    Graph g(10);
    g.add_edge(0, 1, 1);  g.add_edge(1, 2, 1);
    g.add_edge(2, 3, 1);  g.add_edge(3, 4, 1);
    g.add_edge(4, 9, 1);
    g.add_edge(0, 5, 1);  g.add_edge(5, 6, 1);
    g.add_edge(6, 7, 1);  g.add_edge(7, 8, 1);
    g.add_edge(8, 9, 1);
    g.add_edge(2, 8, 3);
    g.add_edge(0, 9, 11);

    auto cwz_r = cwz_nsp(g, 0, 9);
    auto bf = brute_force_nsp(g, 0, 9);
    EXPECT_EQ(cwz_r.shortest_cost, bf.shortest_cost);
    EXPECT_EQ(cwz_r.cost, bf.cost);
    EXPECT_TRUE(check_nsp_path(g, 0, 9, cwz_r.cost, cwz_r.edges));
}

TEST(CwzNsp, MatchesBruteForceOnGeneratedGraphs) {
    // Strong correctness check: cross-validate CWZ against brute force on
    // 40 seeded random graphs from every generator family.
    std::mt19937 rng(0xC0DE);
    int verified_nsp = 0;
    int compared = 0;
    for (int trial = 0; trial < 40; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int n = std::uniform_int_distribution<int>(4, 10)(rng);
        Graph g(0);
        switch (kind) {
            case 0: g = gen::erdos_renyi(n, 0.40, 9, rng); break;
            case 1: g = gen::random_dag(n, 0.45, 9, rng); break;
            case 2: g = gen::layered(std::max(2, n / 3), std::max(1, (n - 2) / 3),
                                     0.5, 0.15, 9, rng); break;
            case 3: g = gen::grid(std::max(2, (int)std::sqrt((double)n)),
                                  std::max(2, (int)std::sqrt((double)n)), 9, rng); break;
        }
        if (g.num_vertices() < 2) continue;
        VertexId t = g.num_vertices() - 1;
        auto cwz_r = cwz_nsp(g, 0, t);
        auto bf = brute_force_nsp(g, 0, t);
        EXPECT_EQ(cwz_r.shortest_cost, bf.shortest_cost)
            << "trial " << trial << " kind=" << kind << " n=" << g.num_vertices();
        EXPECT_EQ(cwz_r.cost, bf.cost)
            << "trial " << trial << " kind=" << kind << " n=" << g.num_vertices();
        if (cwz_r.cost < kInfWeight) {
            EXPECT_TRUE(check_nsp_path(g, 0, t, cwz_r.cost, cwz_r.edges))
                << "trial " << trial;
            ++verified_nsp;
        }
        ++compared;
    }
    EXPECT_GT(compared, 20);
    EXPECT_GT(verified_nsp, 0);
}
