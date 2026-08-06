# Next-to-Shortest Path on Directed Graphs

Implementation and empirical study of the Chen-Wein-Zhang (2025) polynomial-time
algorithm for the **Next-to-Shortest Path (NSP)** problem on positively-weighted
directed graphs (arXiv:2511.04345).

Praca roczna, UJ, supervisor: Lech Duraj. Author: Ihar Maroz.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Requires CMake ≥ 3.20 and a C++20 compiler (GCC 11+ / Clang 13+).
GoogleTest is fetched automatically by CMake.

## Tools

After building, the following binaries are produced in `build/`:

| Binary | Purpose |
|--------|---------|
| `unit_tests` | 68 GoogleTest unit tests |
| `fuzz_brute_vs_yen` | Differential fuzzer: brute force vs Yen-NSP |
| `fuzz_cwz_vs_brute` | Differential fuzzer: CWZ vs brute force |
| `bench` | Benchmark harness on 5 graph families (`results/bench.csv`) |
| `bench_adversarial` | Diamond-chain adversarial benchmark (`results/bench_adversarial.csv`) |
| `nsp-cli` | Solve NSP on a single graph file |

### CLI usage

```bash
# Read a graph from a file and run CWZ:
./build/nsp-cli --algo cwz --input graph.txt

# Choose source and sink (defaults to s=0, t=n-1):
./build/nsp-cli --algo yen --s 0 --t 5 --input graph.txt

# Read from stdin:
echo "4 4
0 1 1
1 3 1
0 2 2
2 3 1" | ./build/nsp-cli --algo brute
```

Graph format: first line `n m`, then `m` lines of `u v w` (0-indexed,
positive integer weights).

### Reproducing the benchmark

```bash
./build/bench results/bench.csv
./build/bench_adversarial results/bench_adversarial.csv
python3 bench/plots/make_plots.py
```

Plots land in `results/`. The adversarial benchmark intentionally lets
Yen-NSP run for ~60 s on the larger inputs; the full sweep takes a
couple of minutes.

### Running fuzz tests

```bash
./build/fuzz_cwz_vs_brute 1000 10 1001
# Expected: mismatches=0.
```

Arguments are `[num_trials] [max_n] [seed]`. The full campaign reported in
the thesis is 34,000 trials over 35 distinct seeds:

```bash
for s in $(seq 1001 1020); do ./build/fuzz_cwz_vs_brute 1000 10 $s; done
for s in $(seq 2001 2010); do ./build/fuzz_cwz_vs_brute 1000 12 $s; done
for s in $(seq 3001 3005); do ./build/fuzz_cwz_vs_brute  800 14 $s; done
```

## Layout

| Directory | Contents |
|-----------|----------|
| `src/graph/` | `Graph` data structure |
| `src/shortest_path/` | Dijkstra forward/reverse, shortest-path DAG |
| `src/two_vdp/` | 2-VDP-in-DAG (max-flow based) |
| `src/cwz/` | CWZ reductions and layered NSP |
| `src/baseline/` | Brute-force NSP, Yen-NSP |
| `src/cli/` | `nsp-cli` |
| `generators/` | Graph generators (Erdős-Rényi, DAG, layered, grid, scale-free, diamond-chain) |
| `tests/unit/` | Unit tests |
| `tests/correctness/` | Differential fuzzers |
| `bench/` | Benchmark harness + plotting |
| `results/` | Generated CSVs and figures (gitignored) |
| `thesis/` | LaTeX source |

## Thesis

LaTeX source lives in `thesis/`; see `thesis/README.md` for template
candidates. To build (requires `pdflatex` + `bibtex`):

```bash
cd thesis
pdflatex main && bibtex main && pdflatex main && pdflatex main
```

## Status

| Component | Status |
|-----------|--------|
| Graph + Dijkstra + SP-DAG | Done |
| Brute-force NSP oracle | Done |
| Yen-NSP heuristic | Done |
| Graph generators | Done |
| 2-VDP-in-DAG | Done (max-flow based; not Tholey 2012 linear-time, same O(V+E)) |
| `(s,t)`-straight reduction (`NextSP`) | Done (iterative folding; K=5 multi-edge cap, see below) |
| `(s,t)`-layered reduction (`NextSP-Straight`) | Done (removes forward/sideways back-edges, records candidates → strictly layered) |
| CWZ layered algorithm (`NextSP-Layered`) | Done — faithful Lemma 5.3 6-tuple enumeration |
| Benchmark harness + plots | Done |
| Thesis drafts | Done (all 6 chapters, needs supervisor pass) |

Correctness: the full pipeline matches the brute-force oracle with **zero
mismatches across 34,000 random trials** (35 distinct seeds, |V|≤14), and matches Yen on every
benign benchmark instance up to |V|=50. An ablation confirms the 6-tuple
enumeration alone suffices on the strictly-layered graph.

One approximation remains: `reduce_to_straight` keeps up to K=5 cheapest
distinct shortcut weights per vertex pair instead of implementing `NextSP`'s
candidate-recording (K=1 with candidates). K=1 alone misses NSPs; no cap
exhausts memory on dense inputs; K=5 is validated empirically. See
`thesis/chapters/implementation.tex` §"Multi-edge cap".

### A note on complexity

The paper proves a worst-case bound of `O(|V|^4 |E|^3 log|V|)`. That bound
follows from the algorithm's *structure* (which this code now matches), not
from the benchmarks: experiments measure runtime on particular inputs and
cannot verify a worst-case upper bound. What the benchmarks do show is that
the implementation is empirically polynomial with a modest exponent that
depends on the family (log-log fit slope ≈ 1.2 on layered graphs up to ≈ 4.9
on dense Erdős–Rényi) — i.e. far below the worst case on
real inputs. Two caveats on inheriting the paper's exact bound: the max-flow
2-VDP is `O(|V|+|E|)` per call (same class as Tholey, larger constant), and
the `K=5` cap is a non-rigorous bound on the `reduce_to_straight` blow-up
(uncapped it is exponential). Implementing `NextSP`'s candidate-recording
would make the whole pipeline a provable transcription of the paper's
complexity.
