# Next-to-Shortest Path on Directed Graphs

Implementation and empirical study of the Chen-Wein-Zhang (2025) polynomial-time
algorithm for the **Next-to-Shortest Path** problem on positively-weighted
directed graphs (arXiv:2511.04345).

Praca roczna, UJ, supervisor: Lech Duraj. Author: Ihar Maroz.

## Build

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Requires CMake ≥ 3.20 and a C++20 compiler. GoogleTest is fetched via
`FetchContent`.

## Layout

- `src/graph/`        — graph data structure
- `src/shortest_path/` — Dijkstra (forward / reverse), shortest-path DAG
- `src/two_vdp/`      — Tholey 2012 linear-time 2-VDP-in-DAG (later)
- `src/cwz/`          — three-layer CWZ algorithm (later)
- `src/baseline/`     — brute-force NSP, Yen-k=2 (later)
- `src/cli/`          — solver entry point (later)
- `generators/`       — graph generators
- `tests/`            — unit + correctness fuzzing
- `bench/`            — experiment scripts
- `thesis/`           — LaTeX
