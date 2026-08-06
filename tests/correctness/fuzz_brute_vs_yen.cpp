// Differential fuzz: brute force vs Yen-NSP should agree on NSP cost across
// random small graphs. They may return different *paths* of equal cost.
//
// This binary is not part of the CTest suite by default; run manually:
//   ./build/fuzz_brute_vs_yen [num_trials] [max_n] [seed]
// Exits nonzero on mismatch.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

#include "baseline/brute_force.h"
#include "baseline/yen.h"
#include "generators/generators.h"
#include "graph/graph.h"

namespace {

const char* gen_name(int kind) {
    switch (kind) {
        case 0: return "erdos_renyi";
        case 1: return "random_dag";
        case 2: return "layered";
        case 3: return "grid";
        case 4: return "scale_free";
    }
    return "?";
}

cwz::Graph make(int kind, int n, std::mt19937& rng) {
    using namespace cwz;
    switch (kind) {
        case 0: return gen::erdos_renyi(n, 0.35, 10, rng);
        case 1: return gen::random_dag(n, 0.4, 10, rng);
        case 2: {
            int layers = std::max(2, n / 3);
            int width = std::max(1, (n - 2) / layers);
            return gen::layered(layers, width, 0.4, 0.1, 10, rng);
        }
        case 3: {
            int side = std::max(2, (int)std::sqrt((double)n));
            return gen::grid(side, side, 10, rng);
        }
        case 4: {
            int m = std::min(3, std::max(1, n / 5));
            if (n < m + 1) return gen::erdos_renyi(n, 0.3, 10, rng);
            return gen::scale_free(n, m, 10, rng);
        }
    }
    return cwz::Graph(0);
}

}  // namespace

int main(int argc, char** argv) {
    int num_trials = argc > 1 ? std::atoi(argv[1]) : 2000;
    int max_n = argc > 2 ? std::atoi(argv[2]) : 10;
    // base 0: accept decimal and 0x-prefixed hex (base 10 parsed "0x..." as 0).
    std::uint64_t seed = argc > 3 ? std::strtoull(argv[3], nullptr, 0) : 0xC0FFEEULL;
    std::mt19937 rng(static_cast<unsigned>(seed));

    int mismatches = 0;
    int yen_caps = 0;
    int agreements = 0;
    int both_no_nsp = 0;

    for (int trial = 0; trial < num_trials; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 4)(rng);
        int n = std::uniform_int_distribution<int>(4, max_n)(rng);
        cwz::Graph g = make(kind, n, rng);
        if (g.num_vertices() < 2) continue;
        cwz::VertexId t = g.num_vertices() - 1;
        cwz::VertexId s = 0;

        auto bf = cwz::brute_force_nsp(g, s, t, /*vertex_cap=*/40);
        auto yn = cwz::yen_nsp(g, s, t, /*k_max=*/4000);

        if (bf.shortest_cost != yn.shortest_cost) {
            std::fprintf(stderr,
                "trial %d (%s, n=%d): shortest cost mismatch: brute=%lld yen=%lld\n",
                trial, gen_name(kind), n,
                (long long)bf.shortest_cost, (long long)yn.shortest_cost);
            mismatches++;
            continue;
        }
        if (bf.cost == cwz::kInfWeight && yn.cost == cwz::kInfWeight) {
            both_no_nsp++;
            continue;
        }
        if (bf.cost != yn.cost) {
            // Yen could hit k_max and false-negative; treat as soft.
            if (yn.cost == cwz::kInfWeight) {
                yen_caps++;
                std::fprintf(stderr,
                    "trial %d (%s, n=%d): yen capped, brute says NSP=%lld\n",
                    trial, gen_name(kind), n, (long long)bf.cost);
                continue;
            }
            std::fprintf(stderr,
                "trial %d (%s, n=%d): NSP cost mismatch: brute=%lld yen=%lld\n",
                trial, gen_name(kind), n,
                (long long)bf.cost, (long long)yn.cost);
            mismatches++;
            continue;
        }
        agreements++;
    }

    std::printf("trials=%d agreements=%d both_no_nsp=%d yen_caps=%d mismatches=%d\n",
                num_trials, agreements, both_no_nsp, yen_caps, mismatches);
    return mismatches == 0 ? 0 : 1;
}
