#include <gtest/gtest.h>

#include <random>
#include <set>

#include "baseline/brute_force.h"
#include "cwz/reductions.h"
#include "generators/generators.h"
#include "graph/graph.h"

using namespace cwz;

// =============================================================================
// reduce_to_straight: small hand-built cases
// =============================================================================

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
    // The headline test. Chain 0 -> 1 -> 2 -> 3 -> 4 of cost 8, with a direct
    // shortcut 0 -> 4 of cost 3 making the chain off-SP.
    //
    //   dS = [0, 2, 4, 6, 3]
    //   dT = [3, 6, 4, 2, 0]
    //   dS + dT = [3, 8, 8, 8, 3]   so 1, 2, 3 all off SP.
    //
    // Reduced graph should have vertices {0, 4} and represent the chain as
    // an additional edge (0, 4) of weight 8 (alongside the direct (0, 4, 3)).
    // Crucially, the cost-8 path-through-chain must survive the reduction
    // because that's the only NSP available in the original graph.
    //
    // A single-pass reduce_to_straight that only generates shortcuts through
    // ONE folded vertex at a time loses this; an iterative version captures it.
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

    // The reduced graph's NSP cost must match the original's. If the
    // multi-vertex chain is lost, NSP cost will collapse to kInfWeight
    // (no NSP) -- which is wrong.
    auto bf_reduced = brute_force_nsp(r.g_prime, r.s_prime, r.t_prime);
    EXPECT_EQ(bf_reduced.shortest_cost, 3);
    EXPECT_EQ(bf_reduced.cost, 8) << "multi-vertex off-SP chain was lost";
}

TEST(ReduceToStraight, MultipleDistinctOffSpPathsBecomeDistinctEdges) {
    // 0 has direct edge to 4 (cost 1, shortest). Plus three off-SP detours
    // through vertices 1, 2, 3 with different costs:
    //   0 -> 1 -> 4    cost 2 + 2 = 4
    //   0 -> 2 -> 4    cost 3 + 1 = 4  (same total cost as via 1)
    //   0 -> 3 -> 4    cost 4 + 5 = 9
    // After reduction, the (0, 4) multi-edge family should include at least
    // one edge of weight 4 and one of weight 9. NSP cost = 4.
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
    // Build a graph where after reduction we know an edge in g_prime came from
    // a shortcut through one folded vertex.
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(0, 2, 5);
    g.add_edge(2, 3, 5);
    auto r = reduce_to_straight(g, 0, 3);
    // Every edge in g_prime must have a non-empty expansion, and each EdgeId
    // in the expansion must be a valid edge of the original graph.
    for (EdgeId i = 0; i < r.g_prime.num_edges(); ++i) {
        ASSERT_LT(i, static_cast<EdgeId>(r.path_expand.size()));
        EXPECT_FALSE(r.path_expand[i].empty());
        for (EdgeId orig : r.path_expand[i]) {
            EXPECT_GE(orig, 0);
            EXPECT_LT(orig, g.num_edges());
        }
    }
}

// =============================================================================
// reduce_to_layered: small hand-built cases
// =============================================================================

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
    // 0 -1-> 1 -1-> 2 is the shortest s->t (cost 2). The edge 0 -10-> 2 goes
    // forward in distance (d(0)=0 < d(2)=2) but is not a shortest-path edge, so
    // it violates the (s,t)-layered conditions. NextSP-Straight removes it and
    // records the candidate path Ps->0 o (0,2) o P2->t = [0->2], cost 10.
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
    // 0->1->2->3 forward chain (cost 3). The back-edge 2 -5-> 1 is strictly
    // backward in distance (d(2)=2 > d(1)=1): it is layered-legal and kept,
    // with no candidate recorded.
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 1);
    g.add_edge(2, 3, 1);
    g.add_edge(2, 1, 5);  // strictly-backward back-edge
    auto r = reduce_to_layered(g, 0, 3);
    EXPECT_EQ(r.candidates.size(), 0u);
    EXPECT_EQ(r.g_prime.num_edges(), 4) << "strictly-backward back-edge kept";
}

// =============================================================================
// Property tests across generated graphs
// =============================================================================

TEST(ReduceToStraight, NspCostPreservedAcrossGeneratedGraphs) {
    // For each random graph, compute NSP cost in the original and in the
    // reduced graph; they must agree. This is the strongest correctness check
    // we have for the reduction. With a buggy single-pass reduction that
    // loses multi-vertex chains, this test fails.
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
        // The reduction now REMOVES forward/sideways back-edges and records
        // them as candidates, so the NSP of the original is preserved by the
        // pair (g_prime, candidates), not by g_prime alone. The reduced NSP
        // cost is the min over (a) the NSP within g_prime and (b) the recorded
        // candidate costs.
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

// =============================================================================
// reduce_full: composition sanity
// =============================================================================

TEST(ReduceFull, HandlesGraphWithOffSpChainAndLayerSkip) {
    // Combines both reductions: an off-SP chain (folded by reduce_to_straight)
    // and a layer-skipping forward edge (subdivided by reduce_to_layered).
    Graph g(6);
    // Shortest s=0 -> t=5 of cost 5 via 0 -> 1 -> 2 -> 5
    g.add_edge(0, 1, 1);  g.add_edge(1, 2, 1);  g.add_edge(2, 5, 3);
    // A layer-skipping forward edge 0 -> 5 of weight 5? Wait — that would
    // make 0->5 directly the shortest. Let me use 1 -> 5 forward skip:
    g.add_edge(1, 5, 4);   // dS[1]+4 = 5 = dS[5]: forward, but skips layer
    // Off-SP detour 0 -> 3 -> 4 -> 5 with cost 1 + 1 + 8 = 10
    g.add_edge(0, 3, 1);  g.add_edge(3, 4, 1);  g.add_edge(4, 5, 8);
    auto bf_orig = brute_force_nsp(g, 0, 5);
    ASSERT_EQ(bf_orig.shortest_cost, 5);
    // Multiple non-shortest paths exist. Cheapest NSP: 0 -> 1 -> 2 -> 5 used the
    // forward chain (cost 5); 0 -> 1 -> 5 (cost 5) ties with shortest; NSP must
    // come from elsewhere. The off-SP chain 0 -> 3 -> 4 -> 5 has cost 10.
    // Let's just check the reduction preserves whatever brute says.
    auto r = reduce_full(g, 0, 5);
    auto bf_reduced = brute_force_nsp(r.g_prime, r.s_prime, r.t_prime, 100);
    EXPECT_EQ(bf_orig.shortest_cost, bf_reduced.shortest_cost);
    // NSP cost = min over the layered graph's NSP and the recorded candidates.
    Weight reduced_nsp = bf_reduced.cost;
    for (const auto& c : r.candidates) reduced_nsp = std::min(reduced_nsp, c.cost);
    EXPECT_EQ(bf_orig.cost, reduced_nsp);
}
