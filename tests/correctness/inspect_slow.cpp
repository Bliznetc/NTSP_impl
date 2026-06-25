// Reproduce trial 58 of profile_cwz with seed 4 and dump the reduced graph
// sizes so we can see where the explosion is.

#include <chrono>
#include <cstdio>
#include <random>

#include "cwz/nsp.h"
#include "cwz/reductions.h"
#include "generators/generators.h"
#include "graph/graph.h"

using clk = std::chrono::steady_clock;
double sec(clk::time_point t0) { return std::chrono::duration<double>(clk::now() - t0).count(); }

int main() {
    // Replay the same RNG sequence to reach trial 58.
    std::mt19937 rng(4ULL);
    cwz::Graph g(0);
    int target_trial = 58;
    for (int trial = 0; trial <= target_trial; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int actual_n = std::uniform_int_distribution<int>(4, 10)(rng);
        switch (kind) {
            case 0: g = cwz::gen::erdos_renyi(actual_n, 0.40, 9, rng); break;
            case 1: g = cwz::gen::random_dag(actual_n, 0.45, 9, rng); break;
            case 2: {
                int layers = std::max(2, actual_n / 3);
                int width = std::max(1, (actual_n - 2) / layers);
                g = cwz::gen::layered(layers, width, 0.45, 0.15, 9, rng);
                break;
            }
            case 3: {
                int side = std::max(2, (int)std::sqrt((double)actual_n));
                g = cwz::gen::grid(side, side, 9, rng);
                break;
            }
        }
        if (trial == target_trial) {
            std::printf("Trial %d: kind=%d n=%d m=%d\n",
                        trial, kind, g.num_vertices(), g.num_edges());
        }
    }
    std::printf("--- graph dump ---\n%s\n", g.to_text().c_str());

    cwz::VertexId t = g.num_vertices() - 1;
    auto t0 = clk::now();
    auto r1 = cwz::reduce_to_straight(g, 0, t);
    double t_r1 = sec(t0);
    std::printf("After reduce_to_straight: V=%d E=%d (%.4fs)\n",
                r1.g_prime.num_vertices(), r1.g_prime.num_edges(), t_r1);
    t0 = clk::now();
    auto r2 = cwz::reduce_to_layered(r1.g_prime, r1.s_prime, r1.t_prime);
    double t_r2 = sec(t0);
    std::printf("After reduce_to_layered: V=%d E=%d (%.4fs)\n",
                r2.g_prime.num_vertices(), r2.g_prime.num_edges(), t_r2);
    return 0;
}
