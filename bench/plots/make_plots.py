#!/usr/bin/env python3
"""Read results/bench.csv and produce runtime + agreement plots."""

import sys
from pathlib import Path

try:
    import pandas as pd
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError as e:
    print(f"missing dep: {e}", file=sys.stderr)
    sys.exit(1)


INF_SENTINEL = 2305843009213693951  # mirrors kInfWeight in C++


def main():
    root = Path(__file__).resolve().parent.parent.parent
    csv_path = root / "results" / "bench.csv"
    out_dir = root / "results"
    out_dir.mkdir(exist_ok=True)
    if not csv_path.exists():
        print(f"no benchmark data at {csv_path}", file=sys.stderr)
        sys.exit(1)
    df = pd.read_csv(csv_path)
    df["has_nsp"] = df["nsp_cost"] != INF_SENTINEL
    df["had_path"] = df["shortest_cost"] != INF_SENTINEL
    # Clamp sub-microsecond runtimes (recorded as 0) for log axes.
    TIMER_FLOOR = 1e-6
    df["seconds"] = df["seconds"].clip(lower=TIMER_FLOOR)

    # Runtime plots use only instances with an s->t path.
    solvable = df[df["had_path"]]
    n_drop = len(df) - len(solvable)
    print(f"runtime plots: dropped {n_drop}/{len(df)} rows with no s->t path")

    families = sorted(df["family"].unique())
    algos = ["brute", "yen", "cwz"]
    colors = {"brute": "tab:gray", "yen": "tab:orange", "cwz": "tab:blue"}

    # Plot 1: runtime vs n per family; line = median, band = IQR.
    # Font sizes account for scaling the figure down to \textwidth.
    ncols = 3
    nrows = (len(families) + ncols - 1) // ncols
    fig, axes = plt.subplots(nrows, ncols, figsize=(4.6 * ncols, 3.6 * nrows),
                             sharey=True, squeeze=False)
    for a in axes.flatten():
        a.tick_params(labelsize=15)
    flat_axes = axes.flatten()
    for idx, fam in enumerate(families):
        ax = flat_axes[idx]
        sub = solvable[solvable["family"] == fam]
        for algo in algos:
            ag = sub[sub["algo"] == algo]
            if ag.empty:
                continue
            grouped = ag.groupby("n")["seconds"]
            median = grouped.median()
            q25 = grouped.quantile(0.25)
            q75 = grouped.quantile(0.75)
            n_values = median.index
            ax.fill_between(n_values, q25.values, q75.values,
                            color=colors[algo], alpha=0.2, linewidth=0)
            ax.plot(n_values, median.values, "o-", label=algo, color=colors[algo])
        ax.set_yscale("log")
        ax.set_xlabel("|V| (target)", fontsize=17)
        ax.set_title(fam, fontsize=20)
        ax.grid(True, which="both", alpha=0.3)
        # y-label on the leftmost panel of each row
        if idx % ncols == 0:
            ax.set_ylabel("runtime (s)", fontsize=17)
    # Hide any unused panels on the grid.
    for j in range(len(families), len(flat_axes)):
        flat_axes[j].set_visible(False)
    flat_axes[0].legend(fontsize=16)
    fig.suptitle("NSP algorithm runtime vs graph size (20 trials per point)",
                 fontsize=24)
    fig.tight_layout(rect=[0, 0, 1, 0.97])
    fig.savefig(out_dir / "runtime_by_family.pdf")
    fig.savefig(out_dir / "runtime_by_family.png", dpi=120)
    print(f"wrote {out_dir / 'runtime_by_family.pdf'}")

    # Plot 2: log-log runtime growth for CWZ to estimate the scaling exponent.
    fig, ax = plt.subplots(figsize=(5, 4))
    cwz = solvable[solvable["algo"] == "cwz"]
    # Median, consistent with the rest of the thesis.
    slopes = {}
    for fam in families:
        ag = cwz[cwz["family"] == fam]
        if ag.empty:
            continue
        med = ag.groupby("n_actual")["seconds"].median()
        med = med[med > 0]
        ax.loglog(med.index, med.values, "o-", label=fam)
        if len(med) >= 3:
            x = np.log(med.index.values.astype(float))
            y = np.log(med.values.astype(float))
            slopes[fam] = np.polyfit(x, y, 1)[0]
    if slopes:
        lo, hi = min(slopes.values()), max(slopes.values())
        ax.set_title(f"CWZ runtime log-log; per-family slopes {lo:.1f}-{hi:.1f}")
        print("CWZ log-log slopes (median runtime vs ACTUAL |V|):")
        for fam, s in sorted(slopes.items(), key=lambda kv: kv[1]):
            print(f"  {fam:15s} {s:5.2f}")
    else:
        ax.set_title("CWZ runtime (log-log)")
    ax.set_xlabel("|V|")
    ax.set_ylabel("seconds")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_dir / "cwz_scaling.pdf")
    fig.savefig(out_dir / "cwz_scaling.png", dpi=120)
    print(f"wrote {out_dir / 'cwz_scaling.pdf'}")

    # Plot 3: NSP existence rate per family.
    fig, ax = plt.subplots(figsize=(5, 4))
    # Ground truth from brute force (Yen can false-negative at its k_max cap).
    for fam in families:
        sub = df[(df["family"] == fam) & (df["algo"] == "brute")]
        if sub.empty:
            continue
        rate = sub.groupby("n")["has_nsp"].mean()
        ax.plot(rate.index, rate.values, "o-", label=fam)
    ax.set_xlabel("|V|")
    ax.set_ylabel("fraction of instances with an NSP")
    ax.set_ylim(0, 1.05)
    ax.set_title("NSP existence rate by family")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_dir / "nsp_existence.pdf")
    fig.savefig(out_dir / "nsp_existence.png", dpi=120)
    print(f"wrote {out_dir / 'nsp_existence.pdf'}")

    # Plot 4: agreement matrix between algorithms (where both reported finite cost).
    summary = []
    for fam in families:
        for n in sorted(df["n"].unique()):
            trials = df[(df["family"] == fam) & (df["n"] == n)]
            cwz_t = trials[trials["algo"] == "cwz"].set_index("trial")["nsp_cost"]
            yen_t = trials[trials["algo"] == "yen"].set_index("trial")["nsp_cost"]
            brute_t = trials[trials["algo"] == "brute"].set_index("trial")["nsp_cost"]
            both = cwz_t.index.intersection(yen_t.index)
            if len(both) == 0:
                continue
            agree = sum(cwz_t.loc[i] == yen_t.loc[i] for i in both)
            row = {"family": fam, "n": n, "trials": len(both),
                   "cwz_yen_agree": agree}
            # CWZ vs the exact oracle.
            vs_brute = cwz_t.index.intersection(brute_t.index)
            row["brute_trials"] = len(vs_brute)
            row["cwz_brute_agree"] = sum(cwz_t.loc[i] == brute_t.loc[i]
                                         for i in vs_brute)
            summary.append(row)
    summary_df = pd.DataFrame(summary)
    summary_df.to_csv(out_dir / "agreement_summary.csv", index=False)
    print(f"wrote {out_dir / 'agreement_summary.csv'}")


def plot_adversarial():
    root = Path(__file__).resolve().parent.parent.parent
    csv_path = root / "results" / "bench_adversarial.csv"
    out_dir = root / "results"
    if not csv_path.exists():
        print(f"no adversarial data at {csv_path}", file=sys.stderr)
        return
    df = pd.read_csv(csv_path)
    fig, ax = plt.subplots(figsize=(6, 4.5))
    for algo, marker, color in [("yen", "o", "tab:orange"), ("cwz", "s", "tab:blue")]:
        sub = df[df["algo"] == algo]
        if sub.empty:
            continue
        ax.plot(sub["k"], sub["seconds"], marker + "-", label=algo, color=color)
    ax.set_yscale("log")
    ax.set_xlabel(r"$k$ (number of parallel diamonds; $2^k$ shortest paths)")
    ax.set_ylabel("seconds")
    ax.set_title("Adversarial diamond-chain: Yen vs CWZ")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(out_dir / "adversarial.pdf")
    fig.savefig(out_dir / "adversarial.png", dpi=120)
    print(f"wrote {out_dir / 'adversarial.pdf'}")


if __name__ == "__main__":
    main()
    plot_adversarial()
