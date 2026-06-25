// Differential fuzz: CWZ NSP vs brute-force NSP on random small graphs.
// Both must agree on NSP cost. CWZ may return a different but equal-cost path.
//
// Usage: ./build/fuzz_cwz_vs_brute [num_trials] [max_n] [seed]

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

#include "baseline/brute_force.h"
#include "cwz/nsp.h"
#include "generators/generators.h"
#include "graph/graph.h"

using clk = std::chrono::steady_clock;
static double secs(clk::time_point t0) {
    return std::chrono::duration<double>(clk::now() - t0).count();
}

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
        case 0: return gen::erdos_renyi(n, 0.40, 9, rng);
        case 1: return gen::random_dag(n, 0.45, 9, rng);
        case 2: {
            int layers = std::max(2, n / 3);
            int width = std::max(1, (n - 2) / layers);
            return gen::layered(layers, width, 0.45, 0.15, 9, rng);
        }
        case 3: {
            int side = std::max(2, (int)std::sqrt((double)n));
            return gen::grid(side, side, 9, rng);
        }
    }
    return cwz::Graph(0);
}

}  // namespace

int main(int argc, char** argv) {
    int num_trials = argc > 1 ? std::atoi(argv[1]) : 1000;
    int max_n = argc > 2 ? std::atoi(argv[2]) : 8;
    std::uint64_t seed = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 0xDEADBEEFULL;
    std::mt19937 rng(static_cast<unsigned>(seed));

    int mismatches = 0;
    int agreements = 0;
    int both_no_nsp = 0;
    int slow_skipped = 0;
    const double per_trial_budget = 2.0;  // seconds per algorithm

    for (int trial = 0; trial < num_trials; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int n = std::uniform_int_distribution<int>(4, max_n)(rng);
        cwz::Graph g = make(kind, n, rng);
        if (g.num_vertices() < 2) continue;
        cwz::VertexId t = g.num_vertices() - 1;
        cwz::VertexId s = 0;

        auto t0 = clk::now();
        auto bf = cwz::brute_force_nsp(g, s, t, 40);
        if (secs(t0) > per_trial_budget) { ++slow_skipped; continue; }
        t0 = clk::now();
        auto cw = cwz::cwz_nsp(g, s, t);
        if (secs(t0) > per_trial_budget) {
            std::fprintf(stderr, "trial %d (%s, n=%d, m=%d): cwz exceeded budget\n",
                         trial, gen_name(kind), n, g.num_edges());
            ++slow_skipped;
            continue;
        }

        if (bf.shortest_cost != cw.shortest_cost) {
            std::fprintf(stderr,
                "trial %d (%s, n=%d): shortest cost mismatch: brute=%lld cwz=%lld\n",
                trial, gen_name(kind), n,
                (long long)bf.shortest_cost, (long long)cw.shortest_cost);
            // Dump graph for repro.
            std::fprintf(stderr, "%s\n", g.to_text().c_str());
            mismatches++;
            continue;
        }
        if (bf.cost == cwz::kInfWeight && cw.cost == cwz::kInfWeight) {
            both_no_nsp++;
            continue;
        }
        if (bf.cost != cw.cost) {
            if (mismatches == 0) {
                std::fprintf(stderr,
                    "trial %d (%s, n=%d): NSP cost mismatch: brute=%lld cwz=%lld shortest=%lld\n",
                    trial, gen_name(kind), n,
                    (long long)bf.cost, (long long)cw.cost, (long long)bf.shortest_cost);
                std::fprintf(stderr, "%s\n", g.to_text().c_str());
            }
            mismatches++;
            continue;
        }
        agreements++;
    }

    std::printf("trials=%d agreements=%d both_no_nsp=%d slow_skipped=%d mismatches=%d\n",
                num_trials, agreements, both_no_nsp, slow_skipped, mismatches);
    return mismatches == 0 ? 0 : 1;
}
