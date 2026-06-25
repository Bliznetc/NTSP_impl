// Adversarial benchmark: diamond_chain graphs that have 2^k distinct simple
// shortest paths. Yen-NSP must enumerate all of them before finding the
// (unique) NSP. CWZ's runtime, by contrast, depends only on graph size.
//
// Writes a CSV (one row per algo and k) to argv[1] or stdout.

#include <chrono>
#include <cstdio>
#include <random>

#include "baseline/yen.h"
#include "cwz/nsp.h"
#include "generators/generators.h"
#include "graph/graph.h"

namespace {
using clk = std::chrono::steady_clock;
double seconds_since(clk::time_point t0) {
    return std::chrono::duration<double>(clk::now() - t0).count();
}
}  // namespace

int main(int argc, char** argv) {
    FILE* out = stdout;
    if (argc > 1) {
        out = std::fopen(argv[1], "w");
        if (!out) {
            std::fprintf(stderr, "cannot open %s\n", argv[1]);
            return 1;
        }
    }
    std::fprintf(out, "algo,k,n_vertices,m_edges,seconds,nsp_cost,shortest_cost\n");

    const double yen_budget = 30.0;
    const double cwz_budget = 60.0;
    bool yen_done = false;

    for (int k = 1; k <= 24; ++k) {
        cwz::Graph g = cwz::gen::diamond_chain(k, /*w=*/1, /*nsp_extra=*/1);
        cwz::VertexId s = 0;
        cwz::VertexId t = g.num_vertices() - 1;

        if (!yen_done) {
            auto t0 = clk::now();
            auto r = cwz::yen_nsp(g, s, t, /*k_max=*/(int)((1ULL << k) + 16));
            double secs = seconds_since(t0);
            std::fprintf(out, "yen,%d,%d,%d,%.6f,%lld,%lld\n", k, g.num_vertices(),
                         g.num_edges(), secs, (long long)r.cost,
                         (long long)r.shortest_cost);
            std::fflush(out);
            if (secs > yen_budget) {
                std::fprintf(stderr, "yen exceeded budget at k=%d (%.2fs); stopping yen\n",
                             k, secs);
                yen_done = true;
            }
        }

        {
            auto t0 = clk::now();
            auto r = cwz::cwz_nsp(g, s, t);
            double secs = seconds_since(t0);
            std::fprintf(out, "cwz,%d,%d,%d,%.6f,%lld,%lld\n", k, g.num_vertices(),
                         g.num_edges(), secs, (long long)r.cost,
                         (long long)r.shortest_cost);
            std::fflush(out);
            if (secs > cwz_budget) {
                std::fprintf(stderr, "cwz exceeded budget at k=%d (%.2fs); stopping cwz\n",
                             k, secs);
                break;
            }
        }
    }
    return 0;
}
