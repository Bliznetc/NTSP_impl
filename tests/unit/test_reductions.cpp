#include <gtest/gtest.h>

#include <random>
#include <set>

#include "baseline/brute_force.h"
#include "cwz/reductions.h"
#include "generators/generators.h"
#include "graph/graph.h"

using namespace cwz;

// ===== reduce_to_straight: hand-built cases =====

TEST(ReduceToStraight, KeepsOnlyOnShortestPathVertices) {
    // 0 -1-> 1 -1-> 2 (shortest, cost 2). 0 -5-> 3 -5-> 2 (cost 10, off SP).
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 1);
    g.add_edge(0, 3, 5);
    g.add_edge(3, 2, 5);
    auto r = reduce_to_straight(g, 0, 2);
    // Vertex 3 should be folded; on-SP vertices 0, 1, 2 remain.
    EXPECT_EQ(r.g_prime.num_vertices(), 3);
    // Direct edges + shortcut for 0 -> 3 -> 2 = (0, 2, 10).
    EXPECT_GE(r.g_prime.num_edges(), 2);
}

TEST(ReduceToStraight, KeepsAllWhenAllOnShortestPath) {
    // Two equal-cost shortest paths.
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(0, 2, 1);
    g.add_edge(2, 3, 1);
    auto r = reduce_to_straight(g, 0, 3);
    EXPECT_EQ(r.g_prime.num_vertices(), 4);
    EXPECT_EQ(r.g_prime.num_edges(), 4);
}

TEST(ReduceToStraight, FoldsMultiVertexOffSpChain) {
    // Off-SP chain 0->1->2->3->4 (cost 8) with shortcut 0->4 (cost 3); the
    // chain is the only NSP and must survive the reduction.
    Graph g(5);
    g.add_edge(0, 1, 2);
    g.add_edge(1, 2, 2);
    g.add_edge(2, 3, 2);
    g.add_edge(3, 4, 2);
    g.add_edge(0, 4, 3);

    // Pre-condition: brute force NSP in original is cost 8 via the chain.
    auto bf_orig = brute_force_nsp(g, 0, 4);
    ASSERT_EQ(bf_orig.shortest_cost, 3);
    ASSERT_EQ(bf_orig.cost, 8);

    auto r = reduce_to_straight(g, 0, 4);
    EXPECT_EQ(r.g_prime.num_vertices(), 2);

    auto bf_reduced = brute_force_nsp(r.g_prime, r.s_prime, r.t_prime);
    EXPECT_EQ(bf_reduced.shortest_cost, 3);
    EXPECT_EQ(bf_reduced.cost, 8) << "multi-vertex off-SP chain was lost";
}

TEST(ReduceToStraight, MultipleDistinctOffSpPathsBecomeDistinctEdges) {
    // Direct 0->4 (cost 1), detours via 1 and 2 (cost 4) and 3 (cost 9); NSP = 4.
    Graph g(5);
    g.add_edge(0, 4, 1);  // direct (shortest)
    g.add_edge(0, 1, 2);  g.add_edge(1, 4, 2);  // via 1, cost 4
    g.add_edge(0, 2, 3);  g.add_edge(2, 4, 1);  // via 2, cost 4
    g.add_edge(0, 3, 4);  g.add_edge(3, 4, 5);  // via 3, cost 9

    auto bf_orig = brute_force_nsp(g, 0, 4);
    ASSERT_EQ(bf_orig.shortest_cost, 1);
    ASSERT_EQ(bf_orig.cost, 4);

    auto r = reduce_to_straight(g, 0, 4);
    EXPECT_EQ(r.g_prime.num_vertices(), 2);

    auto bf_reduced = brute_force_nsp(r.g_prime, r.s_prime, r.t_prime);
    EXPECT_EQ(bf_reduced.shortest_cost, 1);
    EXPECT_EQ(bf_reduced.cost, 4);
}

TEST(ReduceToStraight, MapsSAndTCorrectly) {
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(0, 2, 5);
    g.add_edge(2, 3, 5);
    auto r = reduce_to_straight(g, 0, 3);
    EXPECT_NE(r.s_prime, kNoVertex);
    EXPECT_NE(r.t_prime, kNoVertex);
    // s = 0 is on every shortest path -> kept. Map should preserve it.
    EXPECT_EQ(r.vertex_old_to_new[0], r.s_prime);
    EXPECT_EQ(r.vertex_old_to_new[3], r.t_prime);
    EXPECT_EQ(r.vertex_old_to_new[2], kNoVertex);  // vertex 2 folded
}

TEST(ReduceToStraight, PathExpandRoundTripsEdgesBackToOriginal) {
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(0, 2, 5);
    g.add_edge(2, 3, 5);
    auto r = reduce_to_straight(g, 0, 3);
    // Every reduced edge expands to valid original edges.
    for (EdgeId i = 0; i < r.g_prime.num_edges(); ++i) {
        ASSERT_LT(i, static_cast<EdgeId>(r.path_expand.size()));
        EXPECT_FALSE(r.path_expand[i].empty());
        for (EdgeId orig : r.path_expand[i]) {
            EXPECT_GE(orig, 0);
            EXPECT_LT(orig, g.num_edges());
        }
    }
}

// ===== reduce_to_layered: hand-built cases =====

TEST(ReduceToLayered, SubdividesLayerSkippingForwardEdge) {
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 1);
    g.add_edge(2, 3, 1);
    g.add_edge(0, 2, 2);  // forward edge skipping layer 1
    auto r = reduce_to_layered(g, 0, 3);
    EXPECT_EQ(r.g_prime.num_vertices(), 5);
}

TEST(ReduceToLayered, RemovesForwardInDistanceBackEdgeAsCandidate) {
    // 0->2 (cost 10) goes forward in distance but is not an SP edge: removed
    // and recorded as a candidate.
    Graph g(3);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 1);
    g.add_edge(0, 2, 10);
    auto r = reduce_to_layered(g, 0, 2);
    EXPECT_EQ(r.g_prime.num_edges(), 2) << "the violating back-edge must be removed";
    ASSERT_EQ(r.candidates.size(), 1u);
    EXPECT_EQ(r.candidates[0].cost, 10);
}

TEST(ReduceToLayered, KeepsStrictlyBackwardBackEdge) {
    // 2->1 is strictly backward: kept, no candidate.
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 1);
    g.add_edge(2, 3, 1);
    g.add_edge(2, 1, 5);  // strictly-backward back-edge
    auto r = reduce_to_layered(g, 0, 3);
    EXPECT_EQ(r.candidates.size(), 0u);
    EXPECT_EQ(r.g_prime.num_edges(), 4) << "strictly-backward back-edge kept";
}

// ===== Property tests on generated graphs =====

TEST(ReduceToStraight, NspCostPreservedAcrossGeneratedGraphs) {
    // NSP cost must be the same before and after the reduction.
    std::mt19937 rng(0xBADC0DE);
    int compared = 0;
    int with_nsp = 0;
    for (int trial = 0; trial < 30; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int n = std::uniform_int_distribution<int>(4, 8)(rng);
        Graph g(0);
        switch (kind) {
            case 0: g = gen::erdos_renyi(n, 0.35, 9, rng); break;
            case 1: g = gen::random_dag(n, 0.40, 9, rng); break;
            case 2: g = gen::layered(std::max(2, n / 3), std::max(1, (n - 2) / 3),
                                     0.5, 0.10, 9, rng); break;
            case 3: g = gen::grid(std::max(2, (int)std::sqrt((double)n)),
                                  std::max(2, (int)std::sqrt((double)n)), 9, rng); break;
        }
        if (g.num_vertices() < 2) continue;
        VertexId t = g.num_vertices() - 1;
        auto bf_orig = brute_force_nsp(g, 0, t);
        if (bf_orig.shortest_cost >= kInfWeight) continue;  // disconnected
        auto r = reduce_to_straight(g, 0, t);
        if (r.s_prime == kNoVertex || r.t_prime == kNoVertex) continue;
        auto bf_reduced = brute_force_nsp(r.g_prime, r.s_prime, r.t_prime);
        EXPECT_EQ(bf_orig.shortest_cost, bf_reduced.shortest_cost)
            << "trial " << trial << " kind=" << kind << " n=" << g.num_vertices();
        EXPECT_EQ(bf_orig.cost, bf_reduced.cost)
            << "trial " << trial << " kind=" << kind << " n=" << g.num_vertices();
        ++compared;
        if (bf_orig.cost < kInfWeight) ++with_nsp;
    }
    EXPECT_GT(compared, 10) << "too few testable trials";
    EXPECT_GT(with_nsp, 0) << "no trial produced an NSP";
}

TEST(ReduceFull, NspCostPreservedAcrossGeneratedGraphs) {
    // Same property check for the full pipeline (straight + layered).
    std::mt19937 rng(0xFA17D);
    int compared = 0;
    for (int trial = 0; trial < 30; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int n = std::uniform_int_distribution<int>(4, 8)(rng);
        Graph g(0);
        switch (kind) {
            case 0: g = gen::erdos_renyi(n, 0.35, 9, rng); break;
            case 1: g = gen::random_dag(n, 0.40, 9, rng); break;
            case 2: g = gen::layered(std::max(2, n / 3), std::max(1, (n - 2) / 3),
                                     0.5, 0.10, 9, rng); break;
            case 3: g = gen::grid(std::max(2, (int)std::sqrt((double)n)),
                                  std::max(2, (int)std::sqrt((double)n)), 9, rng); break;
        }
        if (g.num_vertices() < 2) continue;
        VertexId t = g.num_vertices() - 1;
        auto bf_orig = brute_force_nsp(g, 0, t);
        if (bf_orig.shortest_cost >= kInfWeight) continue;
        auto r = reduce_full(g, 0, t);
        if (r.s_prime == kNoVertex || r.t_prime == kNoVertex) continue;
        // Reduced NSP = min(NSP of g_prime, candidate costs).
        VertexId cap = static_cast<VertexId>(std::max(30, r.g_prime.num_vertices() + 5));
        auto bf_reduced = brute_force_nsp(r.g_prime, r.s_prime, r.t_prime, cap);
        EXPECT_EQ(bf_orig.shortest_cost, bf_reduced.shortest_cost)
            << "trial " << trial << " kind=" << kind << " n=" << g.num_vertices();
        Weight reduced_nsp = bf_reduced.cost;
        for (const auto& c : r.candidates)
            reduced_nsp = std::min(reduced_nsp, c.cost);
        EXPECT_EQ(bf_orig.cost, reduced_nsp)
            << "trial " << trial << " kind=" << kind << " n=" << g.num_vertices();
        ++compared;
    }
    EXPECT_GT(compared, 10);
}

// ===== reduce_full: composition =====

TEST(ReduceFull, HandlesGraphWithOffSpChainAndLayerSkip) {
    // Off-SP chain plus a layer-skipping forward edge.
    Graph g(6);
    // Shortest s=0 -> t=5 of cost 5 via 0 -> 1 -> 2 -> 5
    g.add_edge(0, 1, 1);  g.add_edge(1, 2, 1);  g.add_edge(2, 5, 3);
    g.add_edge(1, 5, 4);   // dS[1]+4 = 5 = dS[5]: forward, but skips layer
    // Off-SP detour 0 -> 3 -> 4 -> 5 with cost 1 + 1 + 8 = 10
    g.add_edge(0, 3, 1);  g.add_edge(3, 4, 1);  g.add_edge(4, 5, 8);
    auto bf_orig = brute_force_nsp(g, 0, 5);
    ASSERT_EQ(bf_orig.shortest_cost, 5);
    auto r = reduce_full(g, 0, 5);
    auto bf_reduced = brute_force_nsp(r.g_prime, r.s_prime, r.t_prime, 100);
    EXPECT_EQ(bf_orig.shortest_cost, bf_reduced.shortest_cost);
    Weight reduced_nsp = bf_reduced.cost;
    for (const auto& c : r.candidates) reduced_nsp = std::min(reduced_nsp, c.cost);
    EXPECT_EQ(bf_orig.cost, reduced_nsp);
}
