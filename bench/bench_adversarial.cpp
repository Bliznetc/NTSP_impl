// Adversarial benchmark: diamond_chain graphs that have 2^k distinct simple
// shortest paths. Yen-NSP must enumerate all of them before finding the
// (unique) NSP. CWZ's runtime, by contrast, depends only on graph size.
//
// Measurement note: the two algorithms are timed in SEPARATE passes. An earlier
// version interleaved them (yen at k, then cwz at k), which inflated the CWZ
// timings at k=13..15 by 15-50x: each CWZ run started in the allocator/page
// state left behind by a Yen run that had just churned 2^k paths. Running all
// CWZ measurements first, on a cold-but-uncontaminated heap, and repeating each
// measurement, gives the true microsecond-scale figures.
//
// Writes a CSV (one row per algo and k) to argv[1] or stdout.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

#include "baseline/yen.h"
#include "cwz/nsp.h"
#include "generators/generators.h"
#include "graph/graph.h"

namespace {
using clk = std::chrono::steady_clock;
double seconds_since(clk::time_point t0) {
    return std::chrono::duration<double>(clk::now() - t0).count();
}

struct Row {
    int k = 0;
    int n = 0;
    int m = 0;
    double secs = 0.0;
    long long cost = 0;
    long long shortest = 0;
    bool present = false;
};

double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
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

    const int k_min = 1, k_max = 24;
    const double yen_budget = 30.0;
    const double cwz_budget = 60.0;
    const int cwz_reps = 11;  // odd, so the median is an actual observation

    std::vector<Row> cwz_rows(k_max + 1), yen_rows(k_max + 1);

    // ---- Pass 1: CWZ only. No Yen run has touched the heap in this pass. ----
    for (int k = k_min; k <= k_max; ++k) {
        cwz::Graph g = cwz::gen::diamond_chain(k, /*w=*/1, /*nsp_extra=*/1);
        cwz::VertexId s = 0;
        cwz::VertexId t = g.num_vertices() - 1;

        // Warm-up (not timed): pages in the allocator arenas this size needs.
        auto warm = cwz::cwz_nsp(g, s, t);

        std::vector<double> samples;
        samples.reserve(cwz_reps);
        for (int rep = 0; rep < cwz_reps; ++rep) {
            auto t0 = clk::now();
            auto r = cwz::cwz_nsp(g, s, t);
            samples.push_back(seconds_since(t0));
            if (r.cost != warm.cost) {
                std::fprintf(stderr, "cwz nondeterministic at k=%d\n", k);
                return 1;
            }
        }
        double secs = median(std::move(samples));
        cwz_rows[k] = Row{k,
                          g.num_vertices(),
                          g.num_edges(),
                          secs,
                          (long long)warm.cost,
                          (long long)warm.shortest_cost,
                          true};
        if (secs > cwz_budget) {
            std::fprintf(stderr, "cwz exceeded budget at k=%d (%.2fs); stopping cwz\n",
                         k, secs);
            break;
        }
    }

    // ---- Pass 2: Yen only. ----
    for (int k = k_min; k <= k_max; ++k) {
        cwz::Graph g = cwz::gen::diamond_chain(k, /*w=*/1, /*nsp_extra=*/1);
        cwz::VertexId s = 0;
        cwz::VertexId t = g.num_vertices() - 1;

        auto t0 = clk::now();
        auto r = cwz::yen_nsp(g, s, t, /*k_max=*/(int)((1ULL << k) + 16));
        double secs = seconds_since(t0);
        yen_rows[k] = Row{k,
                          g.num_vertices(),
                          g.num_edges(),
                          secs,
                          (long long)r.cost,
                          (long long)r.shortest_cost,
                          true};
        if (secs > yen_budget) {
            std::fprintf(stderr, "yen exceeded budget at k=%d (%.2fs); stopping yen\n",
                         k, secs);
            break;
        }
    }

    std::fprintf(out, "algo,k,n_vertices,m_edges,seconds,nsp_cost,shortest_cost\n");
    for (int k = k_min; k <= k_max; ++k) {
        if (yen_rows[k].present) {
            const Row& y = yen_rows[k];
            std::fprintf(out, "yen,%d,%d,%d,%.6f,%lld,%lld\n", y.k, y.n, y.m, y.secs,
                         y.cost, y.shortest);
        }
        if (cwz_rows[k].present) {
            const Row& c = cwz_rows[k];
            std::fprintf(out, "cwz,%d,%d,%d,%.6f,%lld,%lld\n", c.k, c.n, c.m, c.secs,
                         c.cost, c.shortest);
        }
    }
    std::fflush(out);
    return 0;
}
