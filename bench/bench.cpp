// Benchmark harness for NSP algorithms.
//
// Runs CWZ, Yen-NSP, and (optionally) brute-force on graphs from each generator
// family across a range of sizes. Times each call (wall clock) and writes one
// CSV row per (algorithm, family, n, trial) to stdout. Run with:
//   ./build/bench [out.csv]
//
// We deliberately keep the parameter ranges small enough to finish on a laptop
// in a few minutes. All three algorithms now run across the whole sweep: the
// brute-force oracle prunes with an admissible lower bound, so it is no longer
// limited to tiny n. Instances where t is unreachable from s are redrawn.

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
    // Keep the layer grid roughly square and let BOTH dimensions grow with n.
    // Deriving layers from n and then width from (n-2)/layers compounds two
    // integer divisions, which made width oscillate (4,3,3,3,4,3,4) across the
    // sweep. That flips the graph between deep-and-narrow and shallow-and-wide
    // and changes the back-edge density discontinuously -- at target n=40 it
    // produced only 32 vertices and a graph with so few back-edges that all
    // three algorithms returned faster than at n=30.
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

// `n` is the requested sweep size; `n_actual` is the vertex count the generator
// really produced. They differ substantially for some families (a target of 50
// gives a 8x8=64-vertex grid and a 49-vertex diamond chain), so scaling fits
// must use n_actual -- fitting against the target skews the exponent.
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
                // Condition on instances where t is reachable from s. A graph
                // with no s->t path is not an NSP instance at all -- every
                // algorithm returns immediately after its first Dijkstra, so
                // such instances measure nothing and merely dilute the
                // existence statistics. Sparse families produce them often at
                // small n (13/20 Erdos-Renyi at n=6). We redraw rather than
                // silently keep them; the cap stops this looping forever on a
                // family that genuinely cannot connect s to t.
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

                // Brute force. The oracle prunes with an admissible lower
                // bound, so it is tractable across the whole sweep on every
                // family; the exception is the occasional Erdos-Renyi instance
                // with a huge number of equal-cost shortest paths, which the
                // bound cannot prune (see the note in the experiments chapter).
                if (n <= 70) {
                    auto t0 = clk::now();
                    // vertex_cap 100: some generators overshoot the target n
                    // (grid rounds the side up, so target 70 gives 9x9 = 81).
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
                // CWZ. Cap at the max sweep size; the per-call budget below
                // catches any pathological dense instance.
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
