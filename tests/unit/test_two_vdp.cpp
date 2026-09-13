// Tests for two_vdp_in_dag: hand-built cases, and oracle comparisons against
// the brute-force solver. Every returned pair is checked as a certificate.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "graph/graph.h"
#include "two_vdp/brute_force_two_vdp.h"
#include "two_vdp/two_vdp.h"

using namespace cwz;

namespace {

using EdgeList = std::vector<std::pair<VertexId, VertexId>>;

Graph make_graph(VertexId n, const EdgeList& edges) {
    Graph g(n);
    for (auto [u, v] : edges) g.add_edge(u, v, 1);
    return g;
}

bool guard_rejects(VertexId s1, VertexId t1, VertexId s2, VertexId t2) {
    return s1 == s2 || t1 == t2 || s1 == t2 || s2 == t1;
}

// ---- Certificate checker ----
::testing::AssertionResult valid_pair(const Graph& g, VertexId s1, VertexId t1,
                                      VertexId s2, VertexId t2,
                                      const TwoVdpResult& r) {
    std::vector<int> owner(g.num_vertices(), 0);  // 0 = unused, 1 = P1, 2 = P2
    auto walk = [&](int label, VertexId src, VertexId tgt,
                    const std::vector<EdgeId>& path) -> ::testing::AssertionResult {
        if (owner[src] != 0)
            return ::testing::AssertionFailure()
                   << "p" << label << ": source " << src << " already used by p"
                   << owner[src];
        owner[src] = label;
        VertexId cur = src;
        for (std::size_t i = 0; i < path.size(); ++i) {
            EdgeId eid = path[i];
            if (eid < 0 || eid >= g.num_edges())
                return ::testing::AssertionFailure()
                       << "p" << label << ": edge id " << eid << " out of range";
            const Edge& e = g.edge(eid);
            if (e.src != cur)
                return ::testing::AssertionFailure()
                       << "p" << label << ": edge #" << i << " (id " << eid << ", "
                       << e.src << "->" << e.dst << ") does not start at current vertex "
                       << cur;
            cur = e.dst;
            if (owner[cur] != 0)
                return ::testing::AssertionFailure()
                       << "p" << label << ": vertex " << cur << " already used by p"
                       << owner[cur] << " (non-simple or non-disjoint)";
            owner[cur] = label;
        }
        if (cur != tgt)
            return ::testing::AssertionFailure()
                   << "p" << label << ": ends at " << cur << ", expected " << tgt;
        return ::testing::AssertionSuccess();
    };
    auto a = walk(1, s1, t1, r.p1);
    if (!a) return a;
    return walk(2, s2, t2, r.p2);
}

// Checks feasibility and validates any returned certificate.
::testing::AssertionResult query_is(bool expected, const Graph& g, VertexId s1,
                                    VertexId t1, VertexId s2, VertexId t2) {
    auto r = two_vdp_in_dag(g, s1, t1, s2, t2);
    if (r.has_value() != expected)
        return ::testing::AssertionFailure()
               << "query (s1=" << s1 << ", t1=" << t1 << ", s2=" << s2 << ", t2=" << t2
               << ") returned " << (r ? "feasible" : "nullopt") << ", expected "
               << (expected ? "feasible" : "nullopt") << "\ngraph:\n"
               << g.to_text();
    if (r) {
        auto v = valid_pair(g, s1, t1, s2, t2, *r);
        if (!v)
            return ::testing::AssertionFailure()
                   << "query (s1=" << s1 << ", t1=" << t1 << ", s2=" << s2
                   << ", t2=" << t2 << ") returned an invalid certificate: "
                   << v.message() << "\ngraph:\n"
                   << g.to_text();
    }
    return ::testing::AssertionSuccess();
}

::testing::AssertionResult matches_oracle(const Graph& g, VertexId s1, VertexId t1,
                                          VertexId s2, VertexId t2) {
    return query_is(brute_force_two_vdp_feasible(g, s1, t1, s2, t2), g, s1, t1, s2, t2);
}

// Relabel vertices by perm (new id of old v is perm[v]).
Graph relabel(VertexId n, const EdgeList& edges, const std::vector<VertexId>& perm) {
    Graph g(n);
    for (auto [u, v] : edges) g.add_edge(perm[u], perm[v], 1);
    return g;
}

}  // namespace

// ===== Guards: shared terminal -> nullopt =====

TEST(TwoVdp, GuardSameSources) {
    Graph g = make_graph(3, {{0, 1}, {0, 2}});
    EXPECT_FALSE(two_vdp_in_dag(g, 0, 1, 0, 2).has_value());
}

TEST(TwoVdp, GuardSameTargets) {
    Graph g = make_graph(3, {{0, 2}, {1, 2}});
    EXPECT_FALSE(two_vdp_in_dag(g, 0, 2, 1, 2).has_value());
}

TEST(TwoVdp, GuardS1EqualsT2) {
    // P1 = 0->1 exists, P2 = 2->0 exists, but they share vertex 0.
    Graph g = make_graph(3, {{0, 1}, {2, 0}});
    EXPECT_FALSE(two_vdp_in_dag(g, 0, 1, 2, 0).has_value());
}

TEST(TwoVdp, GuardS2EqualsT1) {
    Graph g = make_graph(3, {{0, 1}, {1, 2}});
    EXPECT_FALSE(two_vdp_in_dag(g, 0, 1, 1, 2).has_value());
}

TEST(TwoVdp, GuardAppliesToDegeneratePairs) {
    Graph g = make_graph(3, {{0, 1}, {1, 2}});
    EXPECT_FALSE(two_vdp_in_dag(g, 0, 0, 0, 0).has_value());  // everything equal
    EXPECT_FALSE(two_vdp_in_dag(g, 0, 0, 0, 1).has_value());  // s1==s2
    EXPECT_FALSE(two_vdp_in_dag(g, 0, 0, 1, 0).has_value());  // t1==t2, s1==t2
    EXPECT_FALSE(two_vdp_in_dag(g, 1, 1, 0, 1).has_value());  // t1==t2, s2==t1
    EXPECT_FALSE(two_vdp_in_dag(g, 0, 1, 1, 1).has_value());  // s2==t1
}

// ===== Input validation =====

// Professor's counterexample with A<->B is a 2-cycle, so the input is rejected.
TEST(TwoVdp, CyclicProfessorGadgetIsRejectedAsNonDag) {
    enum { S1 = 0, S2 = 1, A = 2, B = 3, T1 = 4, T2 = 5 };
    Graph g(6);
    g.add_edge(S1, A, 1);
    g.add_edge(S2, B, 1);
    g.add_edge(A, T2, 1);
    g.add_edge(B, T1, 1);
    g.add_edge(A, B, 1);
    g.add_edge(B, A, 1);
    EXPECT_THROW(two_vdp_in_dag(g, S1, T1, S2, T2), std::invalid_argument)
        << "cyclic input must be rejected, not answered";
}

// Self-loops are ignored, not treated as cycles.
TEST(TwoVdp, SelfLoopsAreIgnored) {
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 1, 1);
    g.add_edge(2, 3, 1);
    g.add_edge(2, 2, 1);
    std::optional<TwoVdpResult> r;
    ASSERT_NO_THROW(r = two_vdp_in_dag(g, 0, 1, 2, 3));
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(valid_pair(g, 0, 1, 2, 3, *r));
}

// Out-of-range terminals throw, but the guard is evaluated first.
TEST(TwoVdp, OutOfRangeTerminalsThrowAfterGuard) {
    Graph g(3);
    g.add_edge(0, 1, 1);
    EXPECT_THROW(two_vdp_in_dag(g, 0, 1, 2, 7), std::out_of_range);
    EXPECT_THROW(two_vdp_in_dag(g, -1, 1, 2, 2), std::out_of_range);
    EXPECT_FALSE(two_vdp_in_dag(g, 9, 1, 9, 2).has_value());  // s1 == s2 guard
    Graph empty;
    EXPECT_THROW(two_vdp_in_dag(empty, 0, 1, 2, 3), std::out_of_range);
}

// ===== Degenerate pairs (s1==t1 or s2==t2) =====

TEST(TwoVdp, BothPairsDegenerateNoEdges) {
    Graph g(2);
    auto r = two_vdp_in_dag(g, 0, 0, 1, 1);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->p1.empty());
    EXPECT_TRUE(r->p2.empty());
    EXPECT_TRUE(valid_pair(g, 0, 0, 1, 1, *r));
}

TEST(TwoVdp, BothPairsDegenerateInDenseGraph) {
    EdgeList es;
    for (VertexId u = 0; u < 5; ++u)
        for (VertexId v = u + 1; v < 5; ++v) es.push_back({u, v});
    Graph g = make_graph(5, es);
    for (VertexId a = 0; a < 5; ++a)
        for (VertexId b = 0; b < 5; ++b) {
            if (a == b) continue;
            auto r = two_vdp_in_dag(g, a, a, b, b);
            ASSERT_TRUE(r.has_value()) << a << "," << b;
            EXPECT_TRUE(r->p1.empty());
            EXPECT_TRUE(r->p2.empty());
        }
}

TEST(TwoVdp, FirstPairDegenerateSecondDetoursAroundIt) {
    // 1->0->2 and 1->3->2. P1 = {0}; P2 must take 1->3->2.
    Graph g = make_graph(4, {{1, 0}, {0, 2}, {1, 3}, {3, 2}});
    EXPECT_TRUE(query_is(true, g, 0, 0, 1, 2));
    auto r = two_vdp_in_dag(g, 0, 0, 1, 2);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->p1.empty());
    EXPECT_EQ(r->p2.size(), 2u);
}

TEST(TwoVdp, FirstPairDegenerateBlocksSecond) {
    Graph g = make_graph(3, {{1, 0}, {0, 2}});
    EXPECT_TRUE(query_is(false, g, 0, 0, 1, 2));
}

TEST(TwoVdp, SecondPairDegenerateFirstDetoursAroundIt) {
    Graph g = make_graph(4, {{1, 0}, {0, 2}, {1, 3}, {3, 2}});
    EXPECT_TRUE(query_is(true, g, 1, 2, 0, 0));
    auto r = two_vdp_in_dag(g, 1, 2, 0, 0);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->p2.empty());
    EXPECT_EQ(r->p1.size(), 2u);
}

TEST(TwoVdp, SecondPairDegenerateBlocksFirst) {
    Graph g = make_graph(3, {{1, 0}, {0, 2}});
    EXPECT_TRUE(query_is(false, g, 1, 2, 0, 0));
}

TEST(TwoVdp, DegeneratePairWithOtherTargetUnreachable) {
    Graph g(3);
    EXPECT_TRUE(query_is(false, g, 0, 0, 1, 2));
    EXPECT_TRUE(query_is(false, g, 1, 2, 0, 0));
}

TEST(TwoVdp, DegenerateVertexAtEveryTopologicalPosition) {
    // Degenerate vertex d at each position of a chain with a bypass.
    Graph g = make_graph(6, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {0, 5}, {5, 4}});
    for (VertexId d = 1; d <= 5; ++d) {
        if (d == 4) continue;
        EXPECT_TRUE(query_is(true, g, d, d, 0, 4)) << "d=" << d;
        EXPECT_TRUE(query_is(true, g, 0, 4, d, d)) << "d=" << d;
    }
    // Isolated degenerate vertex with no edges at all.
    Graph h = make_graph(3, {{1, 2}});
    EXPECT_TRUE(query_is(true, h, 0, 0, 1, 2));
}

// ===== Reachability =====

TEST(TwoVdp, NoEdgesNonDegenerate) {
    Graph g(4);
    EXPECT_TRUE(query_is(false, g, 0, 1, 2, 3));
}

TEST(TwoVdp, T1Unreachable) {
    Graph g = make_graph(4, {{2, 3}});
    EXPECT_TRUE(query_is(false, g, 0, 1, 2, 3));
}

TEST(TwoVdp, T2Unreachable) {
    Graph g = make_graph(4, {{0, 1}});
    EXPECT_TRUE(query_is(false, g, 0, 1, 2, 3));
}

TEST(TwoVdp, EdgesOnlyInWrongDirection) {
    Graph g = make_graph(4, {{1, 0}, {3, 2}});
    EXPECT_TRUE(query_is(false, g, 0, 1, 2, 3));
    EXPECT_TRUE(query_is(true, g, 1, 0, 3, 2));
}

// ===== Parallel edges =====

TEST(TwoVdp, ParallelEdgesOnBothPaths) {
    Graph g = make_graph(4, {{0, 1}, {0, 1}, {0, 1}, {2, 3}, {2, 3}});
    auto r = two_vdp_in_dag(g, 0, 1, 2, 3);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(valid_pair(g, 0, 1, 2, 3, *r));
    EXPECT_EQ(r->p1.size(), 1u);
    EXPECT_EQ(r->p2.size(), 1u);
}

TEST(TwoVdp, ParallelEdgesGiveNoExtraVertexCapacity) {
    // Doubling every edge through the bottleneck 4 must not let two paths share it.
    Graph g = make_graph(5, {{0, 4}, {0, 4}, {1, 4}, {1, 4}, {4, 2}, {4, 2}, {4, 3}, {4, 3}});
    EXPECT_TRUE(query_is(false, g, 0, 2, 1, 3));
}

TEST(TwoVdp, ParallelEdgesReturnedIdsMatchEndpoints) {
    // Doubled chains: certificate edge ids must chain correctly.
    const VertexId L = 12;
    Graph g(2 * L);
    for (VertexId i = 0; i + 1 < L; ++i) {
        g.add_edge(i, i + 1, 1);
        g.add_edge(L + i, L + i + 1, 1);
        g.add_edge(i, i + 1, 1);
        g.add_edge(L + i, L + i + 1, 1);
    }
    EXPECT_TRUE(query_is(true, g, 0, L - 1, L, 2 * L - 1));
    EXPECT_TRUE(query_is(true, g, L, 2 * L - 1, 0, L - 1));
}

TEST(TwoVdp, ParallelEdgesInCrossingGadget) {
    Graph g = make_graph(6, {{0, 4}, {0, 4}, {0, 5}, {1, 4}, {1, 5}, {1, 5}});
    EXPECT_TRUE(query_is(true, g, 0, 5, 1, 4));
    EXPECT_TRUE(query_is(true, g, 0, 4, 1, 5));
}

// ===== Path forced through the other pair's terminal =====

TEST(TwoVdp, P1ForcedThroughS2) {
    Graph g = make_graph(4, {{0, 2}, {2, 1}, {2, 3}});
    EXPECT_TRUE(query_is(false, g, 0, 1, 2, 3));
    Graph h = make_graph(5, {{0, 2}, {2, 1}, {2, 3}, {0, 4}, {4, 1}});
    EXPECT_TRUE(query_is(true, h, 0, 1, 2, 3));
}

TEST(TwoVdp, P1ForcedThroughT2) {
    Graph g = make_graph(4, {{0, 2}, {2, 1}, {3, 2}});
    EXPECT_TRUE(query_is(false, g, 0, 1, 3, 2));
    Graph h = make_graph(5, {{0, 2}, {2, 1}, {3, 2}, {0, 4}, {4, 1}});
    EXPECT_TRUE(query_is(true, h, 0, 1, 3, 2));
}

TEST(TwoVdp, P2ForcedThroughS1) {
    Graph g = make_graph(4, {{2, 0}, {0, 3}, {0, 1}});
    EXPECT_TRUE(query_is(false, g, 0, 1, 2, 3));
    Graph h = make_graph(5, {{2, 0}, {0, 3}, {0, 1}, {2, 4}, {4, 3}});
    EXPECT_TRUE(query_is(true, h, 0, 1, 2, 3));
}

TEST(TwoVdp, P2ForcedThroughT1) {
    Graph g = make_graph(4, {{2, 1}, {1, 3}, {0, 1}});
    EXPECT_TRUE(query_is(false, g, 0, 1, 2, 3));
    Graph h = make_graph(5, {{2, 1}, {1, 3}, {0, 1}, {2, 4}, {4, 3}});
    EXPECT_TRUE(query_is(true, h, 0, 1, 2, 3));
}

TEST(TwoVdp, P1MayNotPassThroughT2EvenWhenP2FinishesLater) {
    // P1's routes pass t2 or P2's vertex 2; feasible only with 0->4.
    Graph g = make_graph(5, {{0, 3}, {3, 4}, {1, 2}, {2, 3}, {0, 2}, {2, 4}});
    EXPECT_TRUE(query_is(false, g, 0, 4, 1, 3));
    g.add_edge(0, 4, 1);
    EXPECT_TRUE(query_is(true, g, 0, 4, 1, 3));
}

// ===== P2 nested between P1's endpoints =====
TEST(TwoVdp, P2NestedBetweenP1Endpoints) {
    // P1 must jump 0->4 over P2 = 1->2->3.
    Graph g = make_graph(5, {{0, 2}, {2, 4}, {0, 4}, {1, 2}, {2, 3}});
    EXPECT_TRUE(query_is(true, g, 0, 4, 1, 3));
    auto r = two_vdp_in_dag(g, 0, 4, 1, 3);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->p1.size(), 1u);
    EXPECT_EQ(r->p2.size(), 2u);
}

TEST(TwoVdp, InterleavedZigZag) {
    // Interleaved even/odd chains with cross shortcuts.
    Graph g = make_graph(10, {{0, 2}, {2, 4}, {4, 6}, {6, 8}, {1, 3}, {3, 5}, {5, 7},
                              {7, 9}, {0, 3}, {2, 5}, {4, 7}, {6, 9}, {1, 4}, {3, 6},
                              {5, 8}, {0, 5}, {1, 8}});
    for (auto q : std::vector<std::array<VertexId, 4>>{
             {0, 8, 1, 9}, {1, 9, 0, 8}, {0, 9, 1, 8}, {0, 7, 1, 8}, {2, 8, 1, 7}}) {
        EXPECT_TRUE(matches_oracle(g, q[0], q[1], q[2], q[3]));
    }
}

// ===== Hand-built larger instances =====

namespace {

std::vector<VertexId> vertex_sequence(const Graph& g, VertexId src, const std::vector<EdgeId>& p) {
    std::vector<VertexId> seq{src};
    for (EdgeId e : p) seq.push_back(g.edge(e).dst);
    return seq;
}

}  // namespace

TEST(TwoVdp, TenVertexDagWithUniqueSolution) {
    // s1=0, s2=1, t1=8, t2=9. Alone, P1 has 10 routes and P2 has 13, and the
    // crossed pairing is routable, but exactly one paired solution exists:
    // 7->8 is the only edge into t1, so P2 must avoid 7 and finish via 3->9.
    Graph g = make_graph(10, {{0, 2}, {0, 3}, {0, 4}, {1, 6}, {1, 7}, {2, 3},
                              {2, 7}, {3, 5}, {3, 7}, {3, 9}, {4, 2}, {4, 3},
                              {5, 7}, {6, 2}, {6, 4}, {6, 5}, {7, 8}, {7, 9}});
    EXPECT_TRUE(query_is(true, g, 0, 8, 1, 9));
    EXPECT_TRUE(matches_oracle(g, 0, 8, 1, 9));
    auto r = two_vdp_in_dag(g, 0, 8, 1, 9);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(vertex_sequence(g, 0, r->p1), (std::vector<VertexId>{0, 2, 7, 8}));
    EXPECT_EQ(vertex_sequence(g, 1, r->p2), (std::vector<VertexId>{1, 6, 4, 3, 9}));
    EXPECT_TRUE(query_is(true, g, 0, 9, 1, 8));  // crossed pairing
}

TEST(TwoVdp, TwelveVertexLadderOnlyCrossedPairingRoutable) {
    // Planar two-row ladder. Top row s1=0 -> 2 -> 4 -> 6 -> t2=10, bottom row
    // s2=1 -> 3 -> 5 -> 7 -> t1=11, hubs 8 and 9 between the rows, plus
    // 0->3, 2->3 and 6->11. P1 and P2 must swap rows; with the terminals on
    // the outer face in the order s1, t2, t1, s2 they cannot avoid each other.
    Graph g = make_graph(12, {{0, 2}, {2, 4}, {4, 6}, {6, 10}, {1, 3}, {3, 5},
                              {5, 7}, {7, 11}, {2, 8}, {3, 8}, {8, 4}, {8, 5},
                              {4, 9}, {5, 9}, {9, 6}, {9, 7}, {0, 3}, {2, 3},
                              {6, 11}});
    EXPECT_EQ(brute_force_path_vertex_sets(g, 0, 11).size(), 27u);
    EXPECT_EQ(brute_force_path_vertex_sets(g, 1, 10).size(), 4u);
    EXPECT_TRUE(query_is(false, g, 0, 11, 1, 10));
    EXPECT_TRUE(matches_oracle(g, 0, 11, 1, 10));
    EXPECT_TRUE(query_is(true, g, 0, 10, 1, 11));  // crossed pairing
    EXPECT_TRUE(query_is(true, g, 1, 11, 0, 10));
}

// ===== One path finishes early =====

TEST(TwoVdp, T1PrecedesS2InEveryTopologicalOrder) {
    // Edge t1->s2: P2 must keep advancing after P1 finishes.
    Graph g = make_graph(5, {{0, 1}, {1, 3}, {3, 4}});
    EXPECT_TRUE(query_is(true, g, 0, 1, 3, 4));
}

TEST(TwoVdp, T2PrecedesS1InEveryTopologicalOrder) {
    Graph g = make_graph(4, {{2, 3}, {3, 0}, {0, 1}});
    EXPECT_TRUE(query_is(true, g, 0, 1, 2, 3));
}

TEST(TwoVdp, FinishedP1WithLongP2Afterwards) {
    // P1 = 0->1. Then t1=1 has edges into every vertex of a long P2 chain.
    const VertexId L = 40;
    Graph g(2 + L);
    g.add_edge(0, 1, 1);
    for (VertexId i = 0; i < L; ++i) {
        g.add_edge(1, 2 + i, 1);
        if (i + 1 < L) g.add_edge(2 + i, 2 + i + 1, 1);
    }
    EXPECT_TRUE(query_is(true, g, 0, 1, 2, 2 + L - 1));
    EXPECT_TRUE(query_is(true, g, 2, 2 + L - 1, 0, 1));
}

TEST(TwoVdp, FinishedP1StillBlocksLaterP2) {
    // t1=2 lies in the middle of the order and on P2's only route.
    Graph g = make_graph(5, {{1, 2}, {0, 2}, {2, 4}});
    EXPECT_TRUE(query_is(false, g, 1, 2, 0, 4));
    g.add_edge(0, 3, 1);
    g.add_edge(3, 4, 1);
    EXPECT_TRUE(query_is(true, g, 1, 2, 0, 4));
}

TEST(TwoVdp, FinishedEarlierHeadWhileOtherPathStillBeforeIt) {
    // P2 = 0->3->4 jumps over P1 = 1->2.
    Graph g = make_graph(5, {{1, 2}, {0, 3}, {3, 4}, {0, 1}, {2, 3}});
    EXPECT_TRUE(query_is(true, g, 1, 2, 0, 4));
    EXPECT_TRUE(query_is(true, g, 0, 4, 1, 2));
}

TEST(TwoVdp, TargetIsASourceVertexWithOutEdges) {
    // t1 has out-edges; a path must not be "extended" past its target.
    Graph g = make_graph(4, {{0, 1}, {1, 2}, {1, 3}, {2, 3}});
    // P1 = 0->1, P2 = 2->3.
    EXPECT_TRUE(query_is(true, g, 0, 1, 2, 3));
}

TEST(TwoVdp, IncomparableTargetAndSourceUnderManyLabelings) {
    // t1 and s2 are incomparable: try every labeling.
    EdgeList es = {{0, 1}, {2, 3}, {3, 4}, {0, 4}, {2, 5}, {5, 1}};
    std::vector<VertexId> perm = {0, 1, 2, 3, 4, 5};
    int checked = 0;
    do {
        Graph g = relabel(6, es, perm);
        for (auto q : std::vector<std::array<VertexId, 4>>{
                 {0, 1, 2, 4}, {2, 4, 0, 1}, {0, 4, 2, 1}, {0, 1, 5, 4}, {3, 4, 0, 1}}) {
            ASSERT_TRUE(matches_oracle(g, perm[q[0]], perm[q[1]], perm[q[2]], perm[q[3]]));
            ++checked;
        }
    } while (std::next_permutation(perm.begin(), perm.end()));
    EXPECT_EQ(checked, 720 * 5);
}

// ===== Crossing gadgets (max-flow pairing gap) =====

TEST(TwoVdp, FalseNegativeCrossingGadgetAllPairingsAllInsertionOrders) {
    // 0->4, 0->5, 1->4, 1->5. Every pairing of {0,1} with {4,5} is feasible.
    EdgeList es = {{0, 4}, {0, 5}, {1, 4}, {1, 5}};
    std::sort(es.begin(), es.end());
    do {
        Graph g = make_graph(6, es);
        EXPECT_TRUE(query_is(true, g, 0, 5, 1, 4));
        EXPECT_TRUE(query_is(true, g, 0, 4, 1, 5));
        EXPECT_TRUE(query_is(true, g, 1, 4, 0, 5));
        EXPECT_TRUE(query_is(true, g, 1, 5, 0, 4));
    } while (std::next_permutation(es.begin(), es.end()));
}

TEST(TwoVdp, DagCrossingOnlyCrossedPairingExists) {
    // 0->2, 1->3. Max-flow from {0,1} to {2,3} is 2, but (0->3, 1->2) is infeasible.
    Graph g = make_graph(4, {{0, 2}, {1, 3}});
    EXPECT_TRUE(query_is(false, g, 0, 3, 1, 2));
    EXPECT_TRUE(query_is(true, g, 0, 2, 1, 3));
}

TEST(TwoVdp, DagProfessorShapedCrossing) {
    // s1=0, s2=1, A=2, B=3, t1=4, t2=5; s1->A, s2->B, A->t2, B->t1, A->B.
    // Paired: P1 = 0->2->3->4 needs B, and P2 from 1 can only reach 4. Infeasible.
    Graph g = make_graph(6, {{0, 2}, {1, 3}, {2, 5}, {3, 4}, {2, 3}});
    EXPECT_TRUE(query_is(false, g, 0, 4, 1, 5));
    // Adding B->t2 makes P2 = 1->3->5 available but P1 still needs 3.
    g.add_edge(3, 5, 1);
    EXPECT_TRUE(query_is(false, g, 0, 4, 1, 5));
    // Adding a private A'->t1 route for P1 makes it feasible.
    g.add_vertex();  // 6
    g.add_edge(0, 6, 1);
    g.add_edge(6, 4, 1);
    EXPECT_TRUE(query_is(true, g, 0, 4, 1, 5));
}

// Professor's gadget with B->A only: no s1->t1 path at all.
TEST(TwoVdp, ProfessorGadgetOrientedBToAIsInfeasible) {
    enum { S1 = 0, S2 = 1, A = 2, B = 3, T1 = 4, T2 = 5 };
    Graph g(6);
    g.add_edge(S1, A, 1);
    g.add_edge(S2, B, 1);
    g.add_edge(A, T2, 1);
    g.add_edge(B, T1, 1);
    g.add_edge(B, A, 1);
    EXPECT_FALSE(two_vdp_in_dag(g, S1, T1, S2, T2).has_value());
}

TEST(TwoVdp, LayeredCrossingEveryPairing) {
    // Complete bipartite layers of width 2, depth 4; all pairings feasible.
    const VertexId W = 2, D = 4;
    EdgeList es;
    for (VertexId l = 0; l + 1 < D; ++l)
        for (VertexId a = 0; a < W; ++a)
            for (VertexId b = 0; b < W; ++b) es.push_back({l * W + a, (l + 1) * W + b});
    std::mt19937 rng(7);
    for (int rep = 0; rep < 20; ++rep) {
        std::shuffle(es.begin(), es.end(), rng);
        Graph g = make_graph(W * D, es);
        VertexId last = (D - 1) * W;
        EXPECT_TRUE(query_is(true, g, 0, last, 1, last + 1));
        EXPECT_TRUE(query_is(true, g, 0, last + 1, 1, last));
    }
}

// ===== Invariance: pair order, insertion order, labeling =====

TEST(TwoVdp, SwappingThePairsPreservesFeasibility) {
    std::mt19937 rng(99);
    for (int it = 0; it < 300; ++it) {
        VertexId n = 2 + static_cast<VertexId>(rng() % 6);
        std::vector<VertexId> order(n);
        for (VertexId i = 0; i < n; ++i) order[i] = i;
        std::shuffle(order.begin(), order.end(), rng);
        EdgeList es;
        for (VertexId i = 0; i < n; ++i)
            for (VertexId j = i + 1; j < n; ++j)
                if (rng() % 2) es.push_back({order[i], order[j]});
        Graph g = make_graph(n, es);
        for (VertexId s1 = 0; s1 < n; ++s1)
            for (VertexId t1 = 0; t1 < n; ++t1)
                for (VertexId s2 = 0; s2 < n; ++s2)
                    for (VertexId t2 = 0; t2 < n; ++t2) {
                        bool a = two_vdp_in_dag(g, s1, t1, s2, t2).has_value();
                        bool b = two_vdp_in_dag(g, s2, t2, s1, t1).has_value();
                        ASSERT_EQ(a, b) << "asymmetric on (" << s1 << "," << t1 << ","
                                        << s2 << "," << t2 << ")\n"
                                        << g.to_text();
                    }
    }
}

TEST(TwoVdp, InsertionOrderAndLabelInvariance) {
    // Shuffled edges and relabelings must match the oracle.
    EdgeList base = {{0, 2}, {0, 3}, {1, 2}, {1, 4}, {2, 5}, {3, 5}, {3, 6},
                     {4, 5}, {4, 6}, {2, 6}, {0, 4}, {1, 3}};
    std::mt19937 rng(2024);
    for (int rep = 0; rep < 40; ++rep) {
        std::vector<VertexId> perm = {0, 1, 2, 3, 4, 5, 6};
        std::shuffle(perm.begin(), perm.end(), rng);
        EdgeList es = base;
        std::shuffle(es.begin(), es.end(), rng);
        Graph g = relabel(7, es, perm);
        for (VertexId s1 = 0; s1 < 7; ++s1)
            for (VertexId t1 = 0; t1 < 7; ++t1)
                for (VertexId s2 = 0; s2 < 7; ++s2)
                    for (VertexId t2 = 0; t2 < 7; ++t2)
                        ASSERT_TRUE(matches_oracle(g, s1, t1, s2, t2));
    }
}

// ===== Larger instances =====

TEST(TwoVdp, LongLadderWithForwardRungs) {
    // Ladder with ids in reverse topological order.
    const VertexId L = 300;
    const VertexId n = 2 * L;
    auto id = [&](int chain, VertexId i) { return n - 1 - (chain * L + i); };
    Graph g(n);
    for (VertexId i = 0; i + 1 < L; ++i) {
        g.add_edge(id(0, i), id(0, i + 1), 1);
        g.add_edge(id(1, i), id(1, i + 1), 1);
        g.add_edge(id(0, i), id(1, i + 1), 1);
        g.add_edge(id(1, i), id(0, i + 1), 1);
    }
    EXPECT_TRUE(query_is(true, g, id(0, 0), id(0, L - 1), id(1, 0), id(1, L - 1)));
    EXPECT_TRUE(query_is(true, g, id(0, 0), id(1, L - 1), id(1, 0), id(0, L - 1)));
    // Degenerate at the far end.
    EXPECT_TRUE(query_is(true, g, id(0, L - 1), id(0, L - 1), id(1, 0), id(1, L - 1)));
}

TEST(TwoVdp, LongChainsMergingAtOneVertex) {
    const VertexId L = 400;
    Graph g(2 * L + 3);
    const VertexId hub = 2 * L, e1 = 2 * L + 1, e2 = 2 * L + 2;
    for (VertexId i = 0; i + 1 < L; ++i) {
        g.add_edge(i, i + 1, 1);
        g.add_edge(L + i, L + i + 1, 1);
    }
    g.add_edge(L - 1, hub, 1);
    g.add_edge(2 * L - 1, hub, 1);
    g.add_edge(hub, e1, 1);
    g.add_edge(hub, e2, 1);
    EXPECT_TRUE(query_is(false, g, 0, e1, L, e2));
    // One late bypass fixes it.
    g.add_edge(L - 1, e1, 1);
    EXPECT_TRUE(query_is(true, g, 0, e1, L, e2));
}

TEST(TwoVdp, WideCompleteLayers) {
    const VertexId W = 25, D = 12;
    Graph g(W * D);
    for (VertexId l = 0; l + 1 < D; ++l)
        for (VertexId a = 0; a < W; ++a)
            for (VertexId b = 0; b < W; ++b) g.add_edge(l * W + a, (l + 1) * W + b, 1);
    VertexId last = (D - 1) * W;
    EXPECT_TRUE(query_is(true, g, 0, last + W - 1, W - 1, last));
    EXPECT_TRUE(query_is(true, g, 3, last + 3, 7, last + 1));
}

TEST(TwoVdp, WideLayersWithSingleVertexWaist) {
    const VertexId W = 20, D = 9, waist = 4;
    // Layer `waist` has exactly one vertex; all others have W.
    std::vector<std::vector<VertexId>> layer(D);
    VertexId n = 0;
    for (VertexId l = 0; l < D; ++l)
        for (VertexId a = 0; a < (l == waist ? 1 : W); ++a) layer[l].push_back(n++);
    Graph g(n);
    for (VertexId l = 0; l + 1 < D; ++l)
        for (VertexId u : layer[l])
            for (VertexId v : layer[l + 1]) g.add_edge(u, v, 1);
    EXPECT_TRUE(query_is(false, g, layer[0][0], layer[D - 1][0], layer[0][1], layer[D - 1][1]));
    // Both endpoints on the same side of the waist: feasible.
    EXPECT_TRUE(query_is(true, g, layer[0][0], layer[3][0], layer[1][5], layer[3][1]));
    EXPECT_TRUE(
        query_is(true, g, layer[5][0], layer[D - 1][0], layer[6][1], layer[D - 1][1]));
}

// ===== Oracle comparisons =====

namespace {

struct OracleStats {
    long queries = 0, feasible = 0, false_pos = 0, false_neg = 0, invalid = 0;
    std::string first_failures;
    int reported = 0;
};

void run_all_quadruples(const Graph& g, OracleStats& st) {
    const VertexId n = g.num_vertices();
    std::vector<std::vector<std::vector<VertexMask>>> paths(n, std::vector<std::vector<VertexMask>>(n));
    for (VertexId s = 0; s < n; ++s)
        for (VertexId t = 0; t < n; ++t) paths[s][t] = brute_force_path_vertex_sets(g, s, t);
    for (VertexId s1 = 0; s1 < n; ++s1)
        for (VertexId t1 = 0; t1 < n; ++t1) {
            if (paths[s1][t1].empty()) {
                // Unreachable pairs are still queried below.
            }
            for (VertexId s2 = 0; s2 < n; ++s2)
                for (VertexId t2 = 0; t2 < n; ++t2) {
                    bool expect = !guard_rejects(s1, t1, s2, t2) &&
                                  any_disjoint_vertex_sets(paths[s1][t1], paths[s2][t2]);
                    auto r = two_vdp_in_dag(g, s1, t1, s2, t2);
                    ++st.queries;
                    st.feasible += expect;
                    std::string why;
                    if (r && !expect) {
                        ++st.false_pos;
                        why = "false positive";
                    } else if (!r && expect) {
                        ++st.false_neg;
                        why = "false negative";
                    } else if (r) {
                        auto v = valid_pair(g, s1, t1, s2, t2, *r);
                        if (!v) {
                            ++st.invalid;
                            why = std::string("invalid certificate: ") + v.message();
                        }
                    }
                    if (!why.empty() && st.reported < 5) {
                        ++st.reported;
                        std::ostringstream os;
                        os << why << " on (s1=" << s1 << ", t1=" << t1 << ", s2=" << s2
                           << ", t2=" << t2 << ")\n"
                           << g.to_text() << "\n";
                        st.first_failures += os.str();
                    }
                }
        }
}

void expect_clean(const OracleStats& st) {
    EXPECT_EQ(st.false_pos, 0) << st.first_failures;
    EXPECT_EQ(st.false_neg, 0) << st.first_failures;
    EXPECT_EQ(st.invalid, 0) << st.first_failures;
    std::printf("[ oracle   ] queries=%ld feasible=%ld fp=%ld fn=%ld invalid=%ld\n",
                st.queries, st.feasible, st.false_pos, st.false_neg, st.invalid);
}

}  // namespace

TEST(TwoVdp, ExhaustiveAllLabeledDagsOnFourVertices) {
    // All DAGs on 4 labeled vertices, all quadruples.
    OracleStats st;
    std::vector<VertexId> perm = {0, 1, 2, 3};
    do {
        for (unsigned mask = 0; mask < (1u << 6); ++mask) {
            EdgeList es;
            int bit = 0;
            for (VertexId i = 0; i < 4; ++i)
                for (VertexId j = i + 1; j < 4; ++j, ++bit)
                    if (mask >> bit & 1) es.push_back({perm[i], perm[j]});
            run_all_quadruples(make_graph(4, es), st);
        }
    } while (std::next_permutation(perm.begin(), perm.end()));
    expect_clean(st);
}

TEST(TwoVdp, ExhaustiveFiveVertexDagsRandomLabeling) {
    // All 5-vertex DAGs under random labelings, all quadruples.
    OracleStats st;
    std::mt19937 rng(5);
    for (unsigned mask = 0; mask < (1u << 10); ++mask) {
        std::vector<VertexId> perm = {0, 1, 2, 3, 4};
        std::shuffle(perm.begin(), perm.end(), rng);
        EdgeList es;
        int bit = 0;
        for (VertexId i = 0; i < 5; ++i)
            for (VertexId j = i + 1; j < 5; ++j, ++bit)
                if (mask >> bit & 1) es.push_back({perm[i], perm[j]});
        std::shuffle(es.begin(), es.end(), rng);
        run_all_quadruples(make_graph(5, es), st);
    }
    expect_clean(st);
}

TEST(TwoVdp, RandomSmallDagsMatchOracle) {
    // Random DAGs (n <= 7, some parallel edges), all quadruples.
    OracleStats st;
    std::mt19937 rng(20260913);
    const double densities[] = {0.15, 0.3, 0.5, 0.75, 0.95};
    for (int it = 0; it < 400; ++it) {
        VertexId n = 1 + static_cast<VertexId>(it % 7);
        std::vector<VertexId> order(n);
        for (VertexId i = 0; i < n; ++i) order[i] = i;
        std::shuffle(order.begin(), order.end(), rng);
        std::bernoulli_distribution take(densities[it % 5]);
        std::bernoulli_distribution dup(0.15);
        EdgeList es;
        for (VertexId i = 0; i < n; ++i)
            for (VertexId j = i + 1; j < n; ++j)
                if (take(rng)) {
                    es.push_back({order[i], order[j]});
                    if (dup(rng)) es.push_back({order[i], order[j]});
                }
        std::shuffle(es.begin(), es.end(), rng);
        run_all_quadruples(make_graph(n, es), st);
    }
    expect_clean(st);
}
