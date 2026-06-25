// Run CWZ on a sequence of generated graphs and print per-trial timing,
// so we can identify pathological inputs.

#include <chrono>
#include <cstdio>
#include <random>

#include "baseline/brute_force.h"
#include "cwz/nsp.h"
#include "cwz/reductions.h"
#include "generators/generators.h"
#include "graph/graph.h"

using clk = std::chrono::steady_clock;
double sec(clk::time_point t0) { return std::chrono::duration<double>(clk::now() - t0).count(); }

int main(int argc, char** argv) {
    int num = argc > 1 ? std::atoi(argv[1]) : 20;
    int n = argc > 2 ? std::atoi(argv[2]) : 10;
    std::uint64_t seed = argc > 3 ? std::strtoull(argv[3], nullptr, 0) : 4ULL;
    std::mt19937 rng(seed);
    for (int trial = 0; trial < num; ++trial) {
        int kind = std::uniform_int_distribution<int>(0, 3)(rng);
        int actual_n = std::uniform_int_distribution<int>(4, n)(rng);
        cwz::Graph g(0);
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
        if (g.num_vertices() < 2) continue;
        cwz::VertexId t = g.num_vertices() - 1;
        std::fprintf(stderr, "[trial %d kind=%d n=%d m=%d STARTING]\n", trial, kind, g.num_vertices(), g.num_edges());
        std::fflush(stderr);
        auto t0 = clk::now();
        auto reduced = cwz::reduce_full(g, 0, t);
        double t_red = sec(t0);
        int rv = reduced.g_prime.num_vertices();
        int re = reduced.g_prime.num_edges();
        t0 = clk::now();
        auto r = cwz::cwz_nsp(g, 0, t);
        double t_cwz = sec(t0);
        t0 = clk::now();
        auto bf = cwz::brute_force_nsp(g, 0, t, 40);
        double t_bf = sec(t0);
        std::printf("trial=%-3d kind=%d n=%d m=%d -> red V=%d E=%d  reduce=%.4fs  cwz=%.4fs  brute=%.4fs  match=%c\n",
                    trial, kind, g.num_vertices(), g.num_edges(),
                    rv, re, t_red, t_cwz, t_bf,
                    (r.cost == bf.cost && r.shortest_cost == bf.shortest_cost) ? 'Y' : 'N');
        std::fflush(stdout);
    }
    return 0;
}
