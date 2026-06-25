// nsp-cli: read a graph from stdin or a file, run one of {cwz, brute, yen},
// print the NSP cost and path. Graph format: first line "n m", then m lines
// "u v w".
//
// Usage:
//   nsp-cli --algo cwz --input graph.txt
//   echo "4 4 0 1 1 1 3 1 0 2 2 2 3 1" | nsp-cli --algo brute

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "baseline/brute_force.h"
#include "baseline/yen.h"
#include "cwz/nsp.h"
#include "graph/graph.h"

namespace {

void usage(const char* prog) {
    std::fprintf(stderr,
        "Usage: %s --algo {cwz|brute|yen} [--input FILE] [--s S] [--t T]\n"
        "  Default: --algo cwz, S=0, T=n-1, read from stdin.\n",
        prog);
}

}  // namespace

int main(int argc, char** argv) {
    std::string algo = "cwz";
    std::string input;
    int sv = 0, tv = -1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--algo" && i + 1 < argc) {
            algo = argv[++i];
        } else if (a == "--input" && i + 1 < argc) {
            input = argv[++i];
        } else if (a == "--s" && i + 1 < argc) {
            sv = std::atoi(argv[++i]);
        } else if (a == "--t" && i + 1 < argc) {
            tv = std::atoi(argv[++i]);
        } else if (a == "-h" || a == "--help") {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }
    std::stringstream buf;
    if (input.empty()) {
        buf << std::cin.rdbuf();
    } else {
        std::ifstream f(input);
        if (!f) {
            std::fprintf(stderr, "cannot open %s\n", input.c_str());
            return 1;
        }
        buf << f.rdbuf();
    }
    cwz::Graph g = cwz::Graph::from_text(buf.str());
    if (tv < 0) tv = g.num_vertices() - 1;

    cwz::NspResult r;
    if (algo == "cwz") {
        r = cwz::cwz_nsp(g, sv, tv);
    } else if (algo == "brute") {
        r = cwz::brute_force_nsp(g, sv, tv, /*vertex_cap=*/40);
    } else if (algo == "yen") {
        r = cwz::yen_nsp(g, sv, tv);
    } else {
        usage(argv[0]);
        return 1;
    }
    if (r.shortest_cost >= cwz::kInfWeight) {
        std::printf("no_path_from_s_to_t\n");
        return 0;
    }
    std::printf("shortest_cost=%lld\n", (long long)r.shortest_cost);
    if (r.cost >= cwz::kInfWeight) {
        std::printf("nsp=none\n");
    } else {
        std::printf("nsp_cost=%lld\n", (long long)r.cost);
        std::printf("nsp_edges=");
        for (cwz::EdgeId eid : r.edges) {
            const cwz::Edge& e = g.edge(eid);
            std::printf(" %d->%d(w=%lld)", e.src, e.dst, (long long)e.w);
        }
        std::printf("\n");
    }
    return 0;
}
