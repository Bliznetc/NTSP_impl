// Benchmark harness for NSP algorithms.
//
// Runs CWZ, Yen-NSP, and (optionally) brute-force on graphs from each generator
// family across a range of sizes. Times each call (wall clock) and writes one
// CSV row per (algorithm, family, n, trial) to stdout. Run with:
//   ./build/bench [out.csv]
//
// We deliberately keep the parameter ranges small enough to finish on a laptop
// in a few minutes. Brute force is capped at n <= 13 to avoid combinatorial
// blowup; Yen at n <= 60 since it can spiral exponentially on many shortest
// paths; CWZ at n <= 30 because the O(V^4 E^3) layered phase is slow.

#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "baseline/brute_force.h"
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
    // Choose (layers, width) so the actual graph size 2 + layers*width is
    // monotone in n. Previously layers=n/3 and width=(n-2)/layers compounded
    // integer-divisions and produced fewer vertices at n=25 than at n=20.
    int layers = std::max(2, n / 4);
    int width = std::max(1, (n - 2) / layers);
    return cwz::gen::layered(layers, width, 0.40, 0.10, 10, rng);
}
cwz::Graph mk_grid(int n, std::mt19937& rng) {
    // ceil(sqrt(n)) so the side length is monotone in n.
    int side = std::max(2, (int)std::ceil(std::sqrt((double)n)));
    return cwz::gen::grid(side, side, 10, rng);
}
cwz::Graph mk_diamond_chain(int n, std::mt19937& /*rng*/) {
    // Diamond chain has 1 + 3*k vertices, so k = max(1, (n-1)/3) gives a
    // graph with approximately n vertices. This family has 2^k distinct
    // shortest s->t paths, making it the worst case for Yen-NSP and the
    // best case for CWZ (Phase 0 catches the unique back-edge in O(V+E)).
    int k = std::max(1, (n - 1) / 3);
    return cwz::gen::diamond_chain(k, /*w=*/1, /*nsp_extra=*/1);
}

double seconds_since(clk::time_point t0) {
    return std::chrono::duration<double>(clk::now() - t0).count();
}

void emit(FILE* out, const std::string& algo, const std::string& family, int n,
          int trial, double secs, long long nsp_cost, long long shortest_cost) {
    std::fprintf(out, "%s,%s,%d,%d,%.6f,%lld,%lld\n",
                 algo.c_str(), family.c_str(), n, trial, secs, nsp_cost, shortest_cost);
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
    std::fprintf(out, "algo,family,n,trial,seconds,nsp_cost,shortest_cost\n");

    std::vector<Family> families = {
        {"erdos_renyi", mk_erdos},
        {"random_dag", mk_dag},
        {"layered", mk_layered},
        {"grid", mk_grid},
        {"diamond_chain", mk_diamond_chain},
    };

    std::vector<int> sizes = {6, 8, 10, 12, 16, 20, 25, 30, 40, 50};
    const int trials_per_size = 20;
    const double budget_seconds = 30.0;  // per (algo, family, n) call

    std::mt19937 rng(0xBEEFCAFE);

    for (const Family& fam : families) {
        for (int n : sizes) {
            for (int trial = 0; trial < trials_per_size; ++trial) {
                cwz::Graph g = fam.make(n, rng);
                if (g.num_vertices() < 2) continue;
                cwz::VertexId s = 0;
                cwz::VertexId t = g.num_vertices() - 1;

                // Brute force (only for very small n; emit timeout for larger).
                if (n <= 13) {
                    auto t0 = clk::now();
                    auto r = cwz::brute_force_nsp(g, s, t, 60);
                    double secs = seconds_since(t0);
                    emit(out, "brute", fam.name, n, trial, secs,
                         (long long)r.cost, (long long)r.shortest_cost);
                }
                // Yen.
                if (n <= 60) {
                    auto t0 = clk::now();
                    auto r = cwz::yen_nsp(g, s, t, 5000);
                    double secs = seconds_since(t0);
                    if (secs > budget_seconds) {
                        emit(out, "yen", fam.name, n, trial, secs, -1, -1);
                    } else {
                        emit(out, "yen", fam.name, n, trial, secs,
                             (long long)r.cost, (long long)r.shortest_cost);
                    }
                }
                // CWZ. Cap at the max sweep size; the per-call budget below
                // catches any pathological dense instance.
                if (n <= 60) {
                    auto t0 = clk::now();
                    auto r = cwz::cwz_nsp(g, s, t);
                    double secs = seconds_since(t0);
                    if (secs > budget_seconds) {
                        emit(out, "cwz", fam.name, n, trial, secs, -1, -1);
                    } else {
                        emit(out, "cwz", fam.name, n, trial, secs,
                             (long long)r.cost, (long long)r.shortest_cost);
                    }
                }
                std::fflush(out);
            }
        }
    }
    return 0;
}
