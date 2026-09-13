// Benchmark: brute force, Yen and CWZ on every generator family and size.
// One CSV row per (algo, family, n, trial). Usage: ./build/bench [out.csv]
// Instances with t unreachable from s are redrawn.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "baseline/brute_force.h"
#include "shortest_path/dijkstra.h"
#include "baseline/yen.h"
#include "cwz/nsp.h"
#include "generators/generators.h"
#include "graph/graph.h"

namespace {

using clk = std::chrono::steady_clock;

struct Family {
    std::string name;
    cwz::Graph (*make)(int n, std::mt19937& rng);
};

cwz::Graph mk_erdos(int n, std::mt19937& rng) {
    return cwz::gen::erdos_renyi(n, 0.30, 10, rng);
}
cwz::Graph mk_dag(int n, std::mt19937& rng) {
    return cwz::gen::random_dag(n, 0.30, 10, rng);
}
cwz::Graph mk_layered(int n, std::mt19937& rng) {
    // Square-ish layer grid, both dimensions growing with n.
    int width = std::max(2, (int)std::lround(std::sqrt((double)n)));
    int layers = std::max(2, (int)std::lround((double)(n - 2) / width));
    return cwz::gen::layered(layers, width, 0.40, 0.10, 10, rng);
}
cwz::Graph mk_grid(int n, std::mt19937& rng) {
    // ceil(sqrt(n)) so the side length is monotone in n.
    int side = std::max(2, (int)std::ceil(std::sqrt((double)n)));
    return cwz::gen::grid(side, side, 10, rng);
}
cwz::Graph mk_diamond_chain(int n, std::mt19937& /*rng*/) {
    // k = (n-1)/3 gives ~n vertices and 2^k shortest paths (Yen's worst case).
    int k = std::max(1, (n - 1) / 3);
    return cwz::gen::diamond_chain(k, /*w=*/1, /*nsp_extra=*/1);
}

double seconds_since(clk::time_point t0) {
    return std::chrono::duration<double>(clk::now() - t0).count();
}

// n is the target size, n_actual the generated one; fits use n_actual.
void emit(FILE* out, const std::string& algo, const std::string& family, int n,
          int n_actual, int trial, double secs, long long nsp_cost,
          long long shortest_cost) {
    std::fprintf(out, "%s,%s,%d,%d,%d,%.6f,%lld,%lld\n",
                 algo.c_str(), family.c_str(), n, n_actual, trial, secs,
                 nsp_cost, shortest_cost);
}

}  // namespace

int main(int argc, char** argv) {
    FILE* out = stdout;
    if (argc > 1) {
        out = std::fopen(argv[1], "w");
        if (!out) {
            std::fprintf(stderr, "cannot open %s for writing\n", argv[1]);
            return 1;
        }
    }
    std::fprintf(out, "algo,family,n,n_actual,trial,seconds,nsp_cost,shortest_cost\n");

    std::vector<Family> families = {
        {"erdos_renyi", mk_erdos},
        {"random_dag", mk_dag},
        {"layered", mk_layered},
        {"grid", mk_grid},
        {"diamond_chain", mk_diamond_chain},
    };

    std::vector<int> sizes = {6, 8, 10, 12, 16, 20, 25, 30, 40, 50, 60, 70};
    const int trials_per_size = 20;
    const double budget_seconds = 30.0;  // per (algo, family, n) call

    std::mt19937 rng(0xBEEFCAFE);

    for (const Family& fam : families) {
        for (int n : sizes) {
            for (int trial = 0; trial < trials_per_size; ++trial) {
                // Redraw until t is reachable from s (at most 100 attempts).
                cwz::Graph g = fam.make(n, rng);
                for (int attempt = 0; attempt < 100; ++attempt) {
                    if (g.num_vertices() >= 2 &&
                        cwz::dijkstra(g, 0).dist[g.num_vertices() - 1] < cwz::kInfWeight) {
                        break;
                    }
                    g = fam.make(n, rng);
                }
                if (g.num_vertices() < 2) continue;
                cwz::VertexId s = 0;
                cwz::VertexId t = g.num_vertices() - 1;

                // Brute force.
                if (n <= 70) {
                    auto t0 = clk::now();
                    // vertex_cap above the largest generated graph (grid overshoots n).
                    auto r = cwz::brute_force_nsp(g, s, t, 150);
                    double secs = seconds_since(t0);
                    emit(out, "brute", fam.name, n, g.num_vertices(), trial, secs,
                         (long long)r.cost, (long long)r.shortest_cost);
                }
                // Yen.
                if (n <= 70) {
                    auto t0 = clk::now();
                    auto r = cwz::yen_nsp(g, s, t, 5000);
                    double secs = seconds_since(t0);
                    if (secs > budget_seconds) {
                        emit(out, "yen", fam.name, n, g.num_vertices(), trial, secs, -1, -1);
                    } else {
                        emit(out, "yen", fam.name, n, g.num_vertices(), trial, secs,
                             (long long)r.cost, (long long)r.shortest_cost);
                    }
                }
                // CWZ.
                if (n <= 70) {
                    auto t0 = clk::now();
                    auto r = cwz::cwz_nsp(g, s, t);
                    double secs = seconds_since(t0);
                    if (secs > budget_seconds) {
                        emit(out, "cwz", fam.name, n, g.num_vertices(), trial, secs, -1, -1);
                    } else {
                        emit(out, "cwz", fam.name, n, g.num_vertices(), trial, secs,
                             (long long)r.cost, (long long)r.shortest_cost);
                    }
                }
                std::fflush(out);
            }
        }
    }
    return 0;
}
