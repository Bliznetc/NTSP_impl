#include <gtest/gtest.h>

#include <functional>
#include <random>

#include "generators/generators.h"
#include "graph/graph.h"
#include "shortest_path/dijkstra.h"

using namespace cwz;

namespace {

// Brute-force shortest-path cost from s to t by simple-path enumeration with
// branch-and-bound pruning. Used as an independent oracle in cross-check tests.
// Worst-case exponential but fast on the small graphs we feed it (n <= 10).
Weight brute_shortest(const Graph& g, VertexId s, VertexId t) {
    Weight best = kInfWeight;
    std::vector<char> on_path(g.num_vertices(), 0);
    on_path[s] = 1;
    std::function<void(VertexId, Weight)> dfs = [&](VertexId u, Weight cost) {
        if (cost >= best) return;
        if (u == t) { best = cost; return; }
        for (EdgeId eid : g.out_edges(u)) {
            const Edge& e = g.edge(eid);
            if (on_path[e.dst]) continue;
            on_path[e.dst] = 1;
            dfs(e.dst, cost + e.w);
            on_path[e.dst] = 0;
        }
    };
    dfs(s, 0);
    return best;
}

// For every vertex v reachable from src, walk back along parent[] to src and
// sum the edge weights. Assert the sum equals dist[v]. This is the key
// invariant Dijkstra must uphold; downstream code (Yen, CWZ, ...) relies on
// it. Returns ::testing::AssertionResult so callers can use EXPECT_TRUE.
::testing::AssertionResult check_dijkstra_parent_consistency(
    const Graph& g, VertexId src, const DijkstraResult& r) {
    for (VertexId v = 0; v < g.num_vertices(); ++v) {
        if (r.dist[v] >= kInfWeight) continue;
        if (v == src) {
            if (r.dist[v] != 0)
                return ::testing::AssertionFailure()
                    << "dist[src=" << src << "] = " << r.dist[v] << " expected 0";
            if (r.parent[v] != kNoEdge)
                return ::testing::AssertionFailure()
                    << "parent[src=" << src << "] should be kNoEdge";
            continue;
        }
        Weight total = 0;
        VertexId cur = v;
        std::vector<char> seen(g.num_vertices(), 0);
        while (cur != src) {
            if (seen[cur])
                return ::testing::AssertionFailure()
                    << "parent[] cycle when tracing back from " << v;
            seen[cur] = 1;
            EdgeId eid = r.parent[cur];
            if (eid == kNoEdge)
                return ::testing::AssertionFailure()
                    << "parent[" << cur << "] = kNoEdge but dist[" << cur
                    << "] = " << r.dist[cur] << " < inf";
            const Edge& e = g.edge(eid);
            if (e.dst != cur)
                return ::testing::AssertionFailure()
                    << "parent[" << cur << "] = edge " << eid
                    << " whose dst=" << e.dst << " != " << cur;
            total += e.w;
            cur = e.src;
        }
        if (total != r.dist[v])
            return ::testing::AssertionFailure()
                << "walked sum=" << total << " for v=" << v
                << " disagrees with dist[v]=" << r.dist[v];
    }
    return ::testing::AssertionSuccess();
}

}  // namespace

TEST(Dijkstra, SingleVertex) {
    Graph g(1);
    auto r = dijkstra(g, 0);
    EXPECT_EQ(r.dist[0], 0);
    EXPECT_EQ(r.parent[0], kNoEdge);
}

TEST(Dijkstra, UnreachableVertexHasInfDistance) {
    Graph g(3);
    g.add_edge(0, 1, 5);
    // vertex 2 isolated
    auto r = dijkstra(g, 0);
    EXPECT_EQ(r.dist[0], 0);
    EXPECT_EQ(r.dist[1], 5);
    EXPECT_EQ(r.dist[2], kInfWeight);
}

TEST(Dijkstra, ChoosesShorterOfTwoPaths) {
    // 0 -10-> 1 -10-> 2
    // 0 ----> 3 ----> 2  (weights 4, 4 = total 8 < 20)
    Graph g(4);
    g.add_edge(0, 1, 10);
    g.add_edge(1, 2, 10);
    g.add_edge(0, 3, 4);
    g.add_edge(3, 2, 4);
    auto r = dijkstra(g, 0);
    EXPECT_EQ(r.dist[2], 8);
}

TEST(Dijkstra, DirectionalityMattersForDirectedGraph) {
    // 0 -> 1 only; reverse should not propagate backward
    Graph g(2);
    g.add_edge(0, 1, 7);
    auto fwd = dijkstra(g, 0);
    EXPECT_EQ(fwd.dist[1], 7);
    auto from1 = dijkstra(g, 1);
    EXPECT_EQ(from1.dist[0], kInfWeight);
}

TEST(DijkstraReverse, ComputesDistanceToTarget) {
    // 0 -> 1 -> 2; distances to 2: from 0 = 5+3 = 8, from 1 = 3, from 2 = 0
    Graph g(3);
    g.add_edge(0, 1, 5);
    g.add_edge(1, 2, 3);
    auto r = dijkstra_reverse(g, 2);
    EXPECT_EQ(r.dist[2], 0);
    EXPECT_EQ(r.dist[1], 3);
    EXPECT_EQ(r.dist[0], 8);
}

TEST(Dijkstra, RecoveredPathCostMatchesDist) {
    // Hand-built graph: walking back through parent[] from each vertex must
    // produce a path whose summed weight equals dist[v]. This is the
    // invariant Yen's NSP solver relies on -- assert in yen.cpp:
    // path_cost(recovered) == r.shortest_cost.
    Graph g(5);
    g.add_edge(0, 1, 3);
    g.add_edge(0, 2, 1);
    g.add_edge(2, 1, 1);  // 0->2->1 cost 2 beats direct 0->1 cost 3
    g.add_edge(1, 3, 4);
    g.add_edge(2, 4, 7);
    g.add_edge(3, 4, 1);
    auto r = dijkstra(g, 0);
    EXPECT_EQ(r.dist[0], 0);
    EXPECT_EQ(r.dist[1], 2);
    EXPECT_EQ(r.dist[2], 1);
    EXPECT_EQ(r.dist[3], 6);
    EXPECT_EQ(r.dist[4], 7);
    EXPECT_TRUE(check_dijkstra_parent_consistency(g, 0, r));
}

TEST(Dijkstra, ParentInvariantHoldsAcrossGeneratedGraphs) {
    // Property check across seeded random graphs from every generator family.
    // For each, run dijkstra(g, 0) and verify the parent[]/dist[] consistency
    // invariant. If this ever fails, a downstream user (Yen, CWZ) will
    // silently produce wrong NSPs.
    std::mt19937 rng(0xD1A57);
    for (int trial = 0; trial < 30; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int n = std::uniform_int_distribution<int>(4, 20)(rng);
        Graph g(0);
        switch (kind) {
            case 0: g = gen::erdos_renyi(n, 0.35, 10, rng); break;
            case 1: g = gen::random_dag(n, 0.40, 10, rng); break;
            case 2: g = gen::grid(std::max(2, (int)std::sqrt((double)n)),
                                  std::max(2, (int)std::sqrt((double)n)), 10, rng); break;
            case 3: g = gen::layered(std::max(2, n / 3), std::max(1, (n - 2) / 3),
                                     0.5, 0.15, 10, rng); break;
        }
        if (g.num_vertices() < 2) continue;
        auto r = dijkstra(g, 0);
        EXPECT_TRUE(check_dijkstra_parent_consistency(g, 0, r))
            << "trial " << trial << " kind=" << kind << " n=" << g.num_vertices();
    }
}

TEST(Dijkstra, MediumHandBuiltAllDistancesCorrect) {
    // 7-vertex graph with multiple alternative routes. Distances from 0
    // computed by hand:
    //   dist[0] = 0
    //   dist[1] = 2  (0->1)
    //   dist[2] = 3  (0->1->2,  beats direct 0->2 cost 5)
    //   dist[3] = 5  (0->1->2->3, beats 0->1->3 cost 6)
    //   dist[4] = 7  (0->1->2->3->4, beats 0->1->2->4 cost 10)
    //   dist[5] = 8  (0->1->2->3->5 cost 8, tied with 0->...->4->5 cost 8)
    //   dist[6] = 9  (0->...->5->6 cost 9, beats 0->...->4->6 cost 11)
    Graph g(7);
    g.add_edge(0, 1, 2);
    g.add_edge(0, 2, 5);
    g.add_edge(1, 2, 1);
    g.add_edge(1, 3, 4);
    g.add_edge(2, 3, 2);
    g.add_edge(2, 4, 7);
    g.add_edge(3, 4, 2);
    g.add_edge(3, 5, 3);
    g.add_edge(4, 5, 1);
    g.add_edge(4, 6, 4);
    g.add_edge(5, 6, 1);
    auto r = dijkstra(g, 0);
    EXPECT_EQ(r.dist[0], 0);
    EXPECT_EQ(r.dist[1], 2);
    EXPECT_EQ(r.dist[2], 3);
    EXPECT_EQ(r.dist[3], 5);
    EXPECT_EQ(r.dist[4], 7);
    EXPECT_EQ(r.dist[5], 8);
    EXPECT_EQ(r.dist[6], 9);
}

TEST(Dijkstra, MultipleEdgesBetweenSameVerticesPicksCheapest) {
    // Three parallel edges from 0 to 1 with weights 10, 3, 7. Dijkstra must
    // identify the cheapest (weight 3). Then 1 -> 2 single edge w=5.
    Graph g(3);
    g.add_edge(0, 1, 10);
    g.add_edge(0, 1, 3);
    g.add_edge(0, 1, 7);
    g.add_edge(1, 2, 5);
    auto r = dijkstra(g, 0);
    EXPECT_EQ(r.dist[1], 3);
    EXPECT_EQ(r.dist[2], 8);
}

TEST(Dijkstra, MatchesBruteForceShortestOnGeneratedGraphs) {
    // For each random graph from every family, run dijkstra(g, 0) and
    // independently compute the s -> t shortest cost via simple-path
    // enumeration. Equal? Then Dijkstra is producing the right number.
    std::mt19937 rng(0xB4006);
    int verified = 0;
    for (int trial = 0; trial < 40; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int n = std::uniform_int_distribution<int>(4, 9)(rng);
        Graph g(0);
        switch (kind) {
            case 0: g = gen::erdos_renyi(n, 0.40, 9, rng); break;
            case 1: g = gen::random_dag(n, 0.45, 9, rng); break;
            case 2: g = gen::grid(std::max(2, (int)std::sqrt((double)n)),
                                  std::max(2, (int)std::sqrt((double)n)), 9, rng); break;
            case 3: g = gen::layered(std::max(2, n / 3), std::max(1, (n - 2) / 3),
                                     0.5, 0.10, 9, rng); break;
        }
        if (g.num_vertices() < 2) continue;
        VertexId t = g.num_vertices() - 1;
        auto r = dijkstra(g, 0);
        Weight bf = brute_shortest(g, 0, t);
        EXPECT_EQ(r.dist[t], bf)
            << "trial " << trial << " kind=" << kind << " n=" << g.num_vertices();
        ++verified;
    }
    EXPECT_GT(verified, 30) << "very few trials produced testable graphs";
}

TEST(Dijkstra, RelaxationInvariantNoEdgeCanShortenFurther) {
    // Textbook Dijkstra correctness: for every edge (u, v, w) with u
    // reachable from s, dist[v] <= dist[u] + w. Equivalently, no edge can
    // relax any vertex further. If this ever fails on any graph, Dijkstra
    // produced a non-shortest distance for v.
    std::mt19937 rng(0xCAFE1);  // "relax" intent, valid hex
    for (int trial = 0; trial < 30; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int n = std::uniform_int_distribution<int>(4, 20)(rng);
        Graph g(0);
        switch (kind) {
            case 0: g = gen::erdos_renyi(n, 0.35, 10, rng); break;
            case 1: g = gen::random_dag(n, 0.40, 10, rng); break;
            case 2: g = gen::grid(std::max(2, (int)std::sqrt((double)n)),
                                  std::max(2, (int)std::sqrt((double)n)), 10, rng); break;
            case 3: g = gen::layered(std::max(2, n / 3), std::max(1, (n - 2) / 3),
                                     0.5, 0.15, 10, rng); break;
        }
        if (g.num_vertices() < 2) continue;
        auto r = dijkstra(g, 0);
        for (EdgeId i = 0; i < g.num_edges(); ++i) {
            const Edge& e = g.edge(i);
            if (r.dist[e.src] >= kInfWeight) continue;
            EXPECT_LE(r.dist[e.dst], r.dist[e.src] + e.w)
                << "trial " << trial << ": edge " << e.src << "->" << e.dst
                << " (w=" << e.w << ") could relax dist[" << e.dst
                << "]=" << r.dist[e.dst]
                << " via dist[" << e.src << "]+w=" << (r.dist[e.src] + e.w);
        }
    }
}

TEST(DijkstraReverse, MatchesForwardOnTransposeShape) {
    // Asymmetric distances: from-s and to-t should differ when graph is asymmetric.
    Graph g(3);
    g.add_edge(0, 1, 1);
    g.add_edge(1, 2, 1);
    g.add_edge(0, 2, 100);
    auto fwd = dijkstra(g, 0);
    auto rev = dijkstra_reverse(g, 2);
    EXPECT_EQ(fwd.dist[2], 2);
    EXPECT_EQ(rev.dist[0], 2);
    EXPECT_EQ(rev.dist[1], 1);
}
