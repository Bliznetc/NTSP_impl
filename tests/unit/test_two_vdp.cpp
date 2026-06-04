#include <gtest/gtest.h>

#include "graph/graph.h"
#include "two_vdp/two_vdp.h"

using namespace cwz;

TEST(TwoVdp, TrivialDisjointPathsExist) {
    // Two completely separate paths: 0->1 and 2->3.
    Graph g(4);
    g.add_edge(0, 1, 1);
    g.add_edge(2, 3, 1);
    auto r = two_vdp_in_dag(g, 0, 1, 2, 3);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->p1.size(), 1u);
    EXPECT_EQ(r->p2.size(), 1u);
}

TEST(TwoVdp, NoFeasiblePairWhenSingleBottleneck) {
    // 0 and 2 both have to use vertex 1 to reach their targets.
    // 0 -> 1 -> 3 ; 2 -> 1 -> 4
    Graph g(5);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 3, 1);
    g.add_edge(2, 1, 1);
    g.add_edge(1, 4, 1);
    auto r = two_vdp_in_dag(g, 0, 3, 2, 4);
    EXPECT_FALSE(r.has_value());
}

TEST(TwoVdp, InfeasibleWhenS2OnAnyS1ToT1Path) {
    // Graph: 0 -> 1, 1 -> 4, 1 -> 3.
    // Query: 2-VDP(s1=0 -> t1=4, s2=1 -> t2=3).
    // Any 0 -> 4 path must pass through vertex 1, which is also the source s2.
    // Therefore no pair of vertex-disjoint paths exists.
    //
    // This test pokes the choice of vertex-split capacity for the prescribed
    // endpoints in the flow network. With capacity 2 (current code) the
    // algorithm spuriously accepts the instance. With capacity 1 it correctly
    // rejects. See discussion in two_vdp.cpp around the cap-selection block.
    Graph g(5);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 4, 1);
    g.add_edge(1, 3, 1);
    auto r = two_vdp_in_dag(g, /*s1=*/0, /*t1=*/4, /*s2=*/1, /*t2=*/3);
    EXPECT_FALSE(r.has_value());
}

// Validate that (p1, p2) constitute proper vertex-disjoint paths from
// (s1 -> t1) and (s2 -> t2) in g. Returns the union of vertices visited
// (useful for additional assertions in the caller).
::testing::AssertionResult check_disjoint_paths(
    const Graph& g, VertexId s1, VertexId t1, VertexId s2, VertexId t2,
    const std::vector<EdgeId>& p1, const std::vector<EdgeId>& p2) {
    std::vector<char> seen(g.num_vertices(), 0);
    auto walk = [&](VertexId src, VertexId tgt, const std::vector<EdgeId>& path,
                    const char* label) -> ::testing::AssertionResult {
        if (seen[src])
            return ::testing::AssertionFailure()
                   << label << ": source vertex " << src << " already on other path";
        seen[src] = 1;
        VertexId cur = src;
        for (EdgeId eid : path) {
            const Edge& e = g.edge(eid);
            if (e.src != cur)
                return ::testing::AssertionFailure()
                       << label << ": edge " << eid << " has src " << e.src
                       << " but current vertex is " << cur;
            cur = e.dst;
            if (seen[cur])
                return ::testing::AssertionFailure()
                       << label << ": vertex " << cur << " visited twice "
                          "(either re-visit within path or shared with other path)";
            seen[cur] = 1;
        }
        if (cur != tgt)
            return ::testing::AssertionFailure()
                   << label << ": path ends at " << cur << " but should end at " << tgt;
        return ::testing::AssertionSuccess();
    };
    auto r1 = walk(s1, t1, p1, "p1");
    if (!r1) return r1;
    return walk(s2, t2, p2, "p2");
}

TEST(TwoVdp, ComplexLayeredCrossingsHasFeasiblePair) {
    // 8-vertex DAG with two "halves" that cross multiple times:
    //   layer 0: s1=0, s2=1
    //   layer 1: 2, 3   (each reachable from both 0 and 1)
    //   layer 2: 4, 5   (each reachable from both 2 and 3)
    //   layer 3: t1=6, t2=7   (each reachable from both 4 and 5)
    // Edge density: 12 directed edges; multiple feasible disjoint pairs exist.
    Graph g(8);
    g.add_edge(0, 2, 1); g.add_edge(0, 3, 1);
    g.add_edge(1, 2, 1); g.add_edge(1, 3, 1);
    g.add_edge(2, 4, 1); g.add_edge(2, 5, 1);
    g.add_edge(3, 4, 1); g.add_edge(3, 5, 1);
    g.add_edge(4, 6, 1); g.add_edge(4, 7, 1);
    g.add_edge(5, 6, 1); g.add_edge(5, 7, 1);

    auto r = two_vdp_in_dag(g, /*s1=*/0, /*t1=*/6, /*s2=*/1, /*t2=*/7);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(check_disjoint_paths(g, 0, 6, 1, 7, r->p1, r->p2));
    // Each path must traverse one layer-1 vertex and one layer-2 vertex, so
    // both have length 3.
    EXPECT_EQ(r->p1.size(), 3u);
    EXPECT_EQ(r->p2.size(), 3u);
}

TEST(TwoVdp, MultipleDistinctFeasiblePairsExistButOnlyOneIsReturned) {
    // Same 8-vertex layered DAG as above. Two structurally distinct
    // vertex-disjoint pairs exist:
    //   Pair A:  p1 = 0->2->4->6   p2 = 1->3->5->7
    //   Pair B:  p1 = 0->3->5->6   p2 = 1->2->4->7
    // (and several mirror images). The 2-VDP routine is a feasibility solver
    // -- it makes no promise about which feasible pair it returns. This test
    // documents that contract: we accept any returned pair as long as it
    // satisfies the disjoint-path invariant. If a future refactor accidentally
    // returns paths that share a vertex or don't connect, this test fails.
    Graph g(8);
    g.add_edge(0, 2, 1); g.add_edge(0, 3, 1);
    g.add_edge(1, 2, 1); g.add_edge(1, 3, 1);
    g.add_edge(2, 4, 1); g.add_edge(2, 5, 1);
    g.add_edge(3, 4, 1); g.add_edge(3, 5, 1);
    g.add_edge(4, 6, 1); g.add_edge(4, 7, 1);
    g.add_edge(5, 6, 1); g.add_edge(5, 7, 1);

    auto r = two_vdp_in_dag(g, 0, 6, 1, 7);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(check_disjoint_paths(g, 0, 6, 1, 7, r->p1, r->p2));

    // Sanity: the pair must be one of the four structurally-valid pairs.
    // We don't pin down which, but we collect the layer-1 vertex of p1 and
    // p2 and check they're distinct (a property any valid pair must have).
    ASSERT_FALSE(r->p1.empty());
    ASSERT_FALSE(r->p2.empty());
    VertexId p1_layer1 = g.edge(r->p1.front()).dst;
    VertexId p2_layer1 = g.edge(r->p2.front()).dst;
    EXPECT_NE(p1_layer1, p2_layer1)
        << "p1 and p2 entered the same layer-1 vertex";
    EXPECT_TRUE(p1_layer1 == 2 || p1_layer1 == 3);
    EXPECT_TRUE(p2_layer1 == 2 || p2_layer1 == 3);
}

TEST(TwoVdp, LongerDisjointPathsViaAlternate) {
    // 0->1->2->3 (path 1)
    // 4->5->6->7 (path 2)
    Graph g(8);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 1);
    g.add_edge(2, 3, 1);
    g.add_edge(4, 5, 1);
    g.add_edge(5, 6, 1);
    g.add_edge(6, 7, 1);
    auto r = two_vdp_in_dag(g, 0, 3, 4, 7);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->p1.size(), 3u);
    EXPECT_EQ(r->p2.size(), 3u);
}
