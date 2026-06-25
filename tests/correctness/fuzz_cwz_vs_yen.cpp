// Differential fuzz: CWZ NSP vs Yen-NSP on random graphs, at sizes where
// brute force is too slow to act as the oracle. We trust Yen (independently
// fuzzed against brute force at smaller sizes via fuzz_brute_vs_yen) and use
// it as the reference here.
//
// Usage: ./build/fuzz_cwz_vs_yen [num_trials] [max_n] [seed]
//
// Note: at large n, individual instances can still be slow if there are many
// shortest paths (Yen) or many back-edges (CWZ). The per-trial time budget is
// 60 seconds; trials that exceed it on either algorithm are skipped with a
// note rather than counted as a mismatch.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

#include "baseline/yen.h"
#include "cwz/nsp.h"
#include "generators/generators.h"
#include "graph/graph.h"

namespace {

const char* gen_name(int kind) {
    switch (kind) {
        case 0: return "erdos_renyi";
        case 1: return "random_dag";
        case 2: return "layered";
        case 3: return "grid";
    }
    return "?";
}

cwz::Graph make(int kind, int n, std::mt19937& rng) {
    using namespace cwz;
    switch (kind) {
        case 0: return gen::erdos_renyi(n, 0.25, 9, rng);
        case 1: return gen::random_dag(n, 0.30, 9, rng);
        case 2: {
            int layers = std::max(2, n / 4);
            int width = std::max(1, (n - 2) / layers);
            return gen::layered(layers, width, 0.40, 0.15, 9, rng);
        }
        case 3: {
            int side = std::max(2, (int)std::sqrt((double)n));
            return gen::grid(side, side, 9, rng);
        }
    }
    return cwz::Graph(0);
}

using clk = std::chrono::steady_clock;
double seconds(clk::time_point t0) {
    return std::chrono::duration<double>(clk::now() - t0).count();
}

}  // namespace

int main(int argc, char** argv) {
    int num_trials = argc > 1 ? std::atoi(argv[1]) : 500;
    int max_n = argc > 2 ? std::atoi(argv[2]) : 20;
    std::uint64_t seed = argc > 3 ? std::strtoull(argv[3], nullptr, 0) : 0xCAFEBABEULL;
    std::mt19937 rng(static_cast<unsigned>(seed));

    int mismatches = 0;
    int agreements = 0;
    int both_no_nsp = 0;
    int budget_exceeded = 0;
    const double budget = 5.0;

    for (int trial = 0; trial < num_trials; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int n = std::uniform_int_distribution<int>(8, max_n)(rng);
        cwz::Graph g = make(kind, n, rng);
        if (g.num_vertices() < 2) continue;
        cwz::VertexId t = g.num_vertices() - 1;
        cwz::VertexId s = 0;

        auto t0 = clk::now();
        auto y = cwz::yen_nsp(g, s, t, 200000);
        double t_yen = seconds(t0);
        if (t_yen > budget) {
            std::fprintf(stderr, "trial %d (%s, n=%d): yen exceeded budget (%.1fs)\n",
                         trial, gen_name(kind), n, t_yen);
            ++budget_exceeded;
            continue;
        }

        t0 = clk::now();
        auto c = cwz::cwz_nsp(g, s, t);
        double t_cwz = seconds(t0);
        if (t_cwz > budget) {
            std::fprintf(stderr, "trial %d (%s, n=%d): cwz exceeded budget (%.1fs)\n",
                         trial, gen_name(kind), n, t_cwz);
            ++budget_exceeded;
            continue;
        }

        if (y.shortest_cost != c.shortest_cost) {
            std::fprintf(stderr,
                "trial %d (%s, n=%d): shortest cost mismatch: yen=%lld cwz=%lld\n",
                trial, gen_name(kind), n,
                (long long)y.shortest_cost, (long long)c.shortest_cost);
            ++mismatches;
            continue;
        }
        if (y.cost == cwz::kInfWeight && c.cost == cwz::kInfWeight) {
            ++both_no_nsp;
            continue;
        }
        if (y.cost != c.cost) {
            if (mismatches == 0) {
                std::fprintf(stderr,
                    "trial %d (%s, n=%d): NSP cost mismatch: yen=%lld cwz=%lld shortest=%lld\n",
                    trial, gen_name(kind), n,
                    (long long)y.cost, (long long)c.cost, (long long)y.shortest_cost);
                std::fprintf(stderr, "%s\n", g.to_text().c_str());
            }
            ++mismatches;
            continue;
        }
        ++agreements;
    }

    std::printf("trials=%d agreements=%d both_no_nsp=%d budget_exceeded=%d mismatches=%d\n",
                num_trials, agreements, both_no_nsp, budget_exceeded, mismatches);
    return mismatches == 0 ? 0 : 1;
}
