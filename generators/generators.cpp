#include "generators/generators.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace cwz::gen {

namespace {

Weight rand_weight(std::mt19937& rng, Weight max_weight) {
    std::uniform_int_distribution<Weight> d(1, max_weight);
    return d(rng);
}

}  // namespace

Graph erdos_renyi(VertexId n, double p, Weight max_weight, std::mt19937& rng) {
    Graph g(n);
    std::bernoulli_distribution coin(p);
    for (VertexId u = 0; u < n; ++u) {
        for (VertexId v = 0; v < n; ++v) {
            if (u == v) continue;
            if (coin(rng)) g.add_edge(u, v, rand_weight(rng, max_weight));
        }
    }
    return g;
}

Graph random_dag(VertexId n, double p, Weight max_weight, std::mt19937& rng) {
    Graph g(n);
    std::bernoulli_distribution coin(p);
    for (VertexId u = 0; u < n; ++u) {
        for (VertexId v = u + 1; v < n; ++v) {
            if (coin(rng)) g.add_edge(u, v, rand_weight(rng, max_weight));
        }
    }
    return g;
}

Graph layered(int layers, int width, double p_forward, double p_back,
              Weight max_weight, std::mt19937& rng) {
    if (layers < 2 || width < 1) throw std::invalid_argument("layered: bad dimensions");
    // s = 0, layer 0 vertices = [1, 1+width), ..., layer L-1 vertices = ..., t = last.
    // We use `layers` interior layers plus s and t as endpoints.
    const VertexId n = 2 + layers * width;
    Graph g(n);
    auto layer_start = [&](int i) -> VertexId { return 1 + i * width; };
    const VertexId s = 0;
    const VertexId t = n - 1;
    (void)s; (void)t;

    std::bernoulli_distribution fwd(p_forward);
    std::bernoulli_distribution back(p_back);

    // s -> layer 0
    for (int j = 0; j < width; ++j) {
        g.add_edge(0, layer_start(0) + j, rand_weight(rng, max_weight));
    }
    // layer i -> layer i+1
    for (int i = 0; i + 1 < layers; ++i) {
        for (int u = 0; u < width; ++u) {
            for (int v = 0; v < width; ++v) {
                if (fwd(rng)) {
                    g.add_edge(layer_start(i) + u, layer_start(i + 1) + v,
                               rand_weight(rng, max_weight));
                }
            }
        }
    }
    // optional back-edges layer i+1 -> layer i
    if (p_back > 0) {
        for (int i = 0; i + 1 < layers; ++i) {
            for (int u = 0; u < width; ++u) {
                for (int v = 0; v < width; ++v) {
                    if (back(rng)) {
                        g.add_edge(layer_start(i + 1) + u, layer_start(i) + v,
                                   rand_weight(rng, max_weight));
                    }
                }
            }
        }
    }
    // last layer -> t
    for (int j = 0; j < width; ++j) {
        g.add_edge(layer_start(layers - 1) + j, n - 1, rand_weight(rng, max_weight));
    }
    return g;
}

Graph grid(int rows, int cols, Weight max_weight, std::mt19937& rng) {
    if (rows < 1 || cols < 1) throw std::invalid_argument("grid: bad dimensions");
    Graph g(rows * cols);
    auto idx = [&](int r, int c) { return r * cols + c; };
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (c + 1 < cols) g.add_edge(idx(r, c), idx(r, c + 1), rand_weight(rng, max_weight));
            if (r + 1 < rows) g.add_edge(idx(r, c), idx(r + 1, c), rand_weight(rng, max_weight));
        }
    }
    return g;
}

Graph diamond_chain(int k, Weight w, Weight nsp_extra) {
    if (k < 1 || w <= 0 || nsp_extra <= 0)
        throw std::invalid_argument("diamond_chain: bad parameters");
    const VertexId n = 1 + 3 * k;
    Graph g(n);
    auto top = [](int i) { return 3 * i - 2; };
    auto bot = [](int i) { return 3 * i - 1; };
    auto merge = [](int i) { return 3 * i; };
    VertexId prev = 0;  // s
    for (int i = 1; i <= k; ++i) {
        g.add_edge(prev, top(i), w);
        g.add_edge(prev, bot(i), w);
        g.add_edge(top(i), merge(i), w);
        g.add_edge(bot(i), merge(i), w);
        prev = merge(i);
    }
    // NSP shortcut: direct s -> t edge whose weight exceeds the shortest.
    g.add_edge(0, n - 1, 2 * k * w + nsp_extra);
    return g;
}

Graph scale_free(VertexId n, int m, Weight max_weight, std::mt19937& rng) {
    if (m < 1 || n < m + 1) throw std::invalid_argument("scale_free: bad parameters");
    Graph g(n);
    // Initial clique of m+1 vertices to seed degree distribution.
    for (VertexId u = 0; u <= m; ++u) {
        for (VertexId v = 0; v <= m; ++v) {
            if (u != v) g.add_edge(u, v, rand_weight(rng, max_weight));
        }
    }
    std::vector<int> degree(n, 0);
    for (EdgeId i = 0; i < g.num_edges(); ++i) {
        degree[g.edge(i).src]++;
        degree[g.edge(i).dst]++;
    }
    std::uniform_int_distribution<int> dir(0, 1);

    for (VertexId v = m + 1; v < n; ++v) {
        int total_degree = 0;
        for (VertexId u = 0; u < v; ++u) total_degree += degree[u];

        std::vector<char> picked(v, 0);
        for (int k = 0; k < m; ++k) {
            std::uniform_int_distribution<int> draw(1, std::max(1, total_degree));
            int r = draw(rng);
            VertexId u = 0;
            int acc = 0;
            for (; u < v; ++u) {
                if (picked[u]) continue;
                acc += degree[u];
                if (acc >= r) break;
            }
            if (u >= v) continue;
            picked[u] = 1;
            if (dir(rng)) {
                g.add_edge(v, u, rand_weight(rng, max_weight));
            } else {
                g.add_edge(u, v, rand_weight(rng, max_weight));
            }
            degree[u]++;
            degree[v]++;
            total_degree -= degree[u] - 1;
            if (total_degree < 1) total_degree = 1;
        }
    }
    return g;
}

}  // namespace cwz::gen
