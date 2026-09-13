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
| `unit_tests` | 110 GoogleTest unit tests |
| `fuzz_brute_vs_yen` | Differential fuzzer: brute force vs Yen-NSP |
| `fuzz_cwz_vs_brute` | Differential fuzzer: CWZ vs brute force |
| `fuzz_cwz_vs_yen` | Differential fuzzer: CWZ vs Yen-NSP on larger graphs |
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

### Running unit tests

```bash
./build/unit_tests                                   # all tests
./build/unit_tests --gtest_filter='TwoVdp.*'         # only 2-VDP
./build/unit_tests --gtest_filter='TwoVdp.Exhaustive*'  # 2-VDP vs brute-force oracle
./build/unit_tests --gtest_list_tests                # list test names
```

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
| `src/shortest_path/` | Dijkstra forward/reverse |
| `src/two_vdp/` | 2-VDP-in-DAG (Fortune–Hopcroft–Wyllie) and a brute-force oracle for it |
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
latexmk -pdf main.tex
```

## Status

| Component | Status |
|-----------|--------|
| Graph + Dijkstra | Done |
| Brute-force NSP oracle | Done |
| Yen-NSP heuristic | Done |
| Graph generators | Done |
| 2-VDP-in-DAG | Done (Fortune–Hopcroft–Wyllie for two paths, `O(|V|^2 + |V||E|)` per call; not Tholey 2012 linear-time) |
| Brute-force 2-VDP oracle | Done |
| `(s,t)`-straight reduction (`NextSP`) | Done (iterative folding) |
| `(s,t)`-layered reduction (`NextSP-Straight`) | Done (removes forward/sideways back-edges, records candidates → strictly layered) |
| CWZ layered algorithm (`NextSP-Layered`) | Done — faithful Lemma 5.3 6-tuple enumeration |
| Benchmark harness + plots | Done |
| Thesis drafts | Done (all 6 chapters, needs supervisor pass) |

Correctness:

- The full pipeline matches the brute-force NSP oracle with **zero mismatches
  across 34,000 random trials** (35 distinct seeds, |V| ≤ 14) and on all 1,200
  benchmark instances (|V| up to 81).
- The 2-VDP solver matches the brute-force 2-VDP oracle on **1,299,749 queries**:
  every terminal quadruple of all DAGs on 4 vertices, all 5-vertex DAGs, and
  400 random DAGs with up to 7 vertices. Every returned pair of paths is also
  checked to be vertex-disjoint with the right endpoints.

### A note on complexity

The paper proves a worst-case bound of `O(|V|^4 |E|^3 log|V|)`. That bound
follows from the algorithm's *structure*, not from the benchmarks: experiments
measure runtime on particular inputs and cannot verify a worst-case upper
bound. What the benchmarks do show is that the implementation is empirically
polynomial with an exponent that depends on the family (log-log fit slope
≈ 1.3 on diamond chains up to ≈ 4.4 on dense Erdős–Rényi), far below the worst
case. The implementation does not attain the paper's exact bound: its 2-VDP
subroutine costs `O(|V|^2 + |V||E|)` per call instead of Tholey's
`O(|V| + |E|)`.
