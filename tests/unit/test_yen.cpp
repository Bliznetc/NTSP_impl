#include <gtest/gtest.h>

#include <cmath>
#include <random>

#include "baseline/brute_force.h"
#include "baseline/yen.h"
#include "generators/generators.h"
#include "graph/graph.h"

using namespace cwz;

namespace {

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

    auto r = yen_nsp(g, 0, 9);
    EXPECT_EQ(r.shortest_cost, 5);
    EXPECT_EQ(r.cost, 6);
    EXPECT_TRUE(check_nsp_path(g, 0, 9, r.cost, r.edges));
}

TEST(YenNsp, DiamondChainK4ExtensiveAlternates) {
    // 16 shortest paths of cost 8; the NSP is the direct edge (cost 9).
    Graph g = gen::diamond_chain(/*k=*/4, /*w=*/1, /*nsp_extra=*/1);
    VertexId t = g.num_vertices() - 1;
    auto r = yen_nsp(g, 0, t);
    EXPECT_EQ(r.shortest_cost, 8);
    EXPECT_EQ(r.cost, 9);
    EXPECT_TRUE(check_nsp_path(g, 0, t, r.cost, r.edges));
    EXPECT_EQ(r.edges.size(), 1u);  // NSP is just the direct shortcut edge
}

TEST(YenNsp, KMaxCapReturnsNoNspGracefully) {
    // k_max below the number of shortest paths: no NSP, not a wrong one.
    Graph g = gen::diamond_chain(/*k=*/4, /*w=*/1, /*nsp_extra=*/1);
    VertexId t = g.num_vertices() - 1;
    auto r = yen_nsp(g, 0, t, /*k_max=*/3);
    EXPECT_EQ(r.shortest_cost, 8);
    EXPECT_EQ(r.cost, kInfWeight) << "k_max cap should produce a false negative, not a wrong NSP";
    EXPECT_TRUE(r.edges.empty());
}

TEST(YenNsp, MatchesBruteForceOnGeneratedGraphs) {
    // Yen vs brute force on random graphs; costs must agree.
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
