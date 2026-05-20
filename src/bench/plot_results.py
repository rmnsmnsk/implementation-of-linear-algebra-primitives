#!/usr/bin/env python3
import json
import matplotlib.pyplot as plt
import numpy as np
import os

RESULTS_PATH = "results.json"
OUTPUT_PNG = "benchmark_results.png"
OUTPUT_TABLE = "results_table.md"

def load_results(path):
    if not os.path.exists(path):
        raise FileNotFoundError(f"File {path} not found. Run python3 run_all.py first")
    with open(path, "r") as f:
        return json.load(f)

def format_time(seconds):
    if seconds is None:
        return "N/A"
    if seconds < 1e-6:
        return f"{seconds*1e9:.1f} ns"
    if seconds < 1e-3:
        return f"{seconds*1e6:.1f} µs"
    if seconds < 1:
        return f"{seconds*1e3:.1f} ms"
    return f"{seconds:.2f} s"

def plot_results(data):
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    ops = ["mm", "mv"]
    titles = {"mm": "Matrix × Matrix", "mv": "Matrix × Vector"}
    colors = {"c": "#2E86AB", "fsharp": "#A23B72"}

    for idx, op in enumerate(ops):
        ax = axes[idx]
        labels = list(data["c"][op].keys())
        sizes = [int(l.split("_")[0]) for l in labels]
        densities = [float(l.split("_")[1]) for l in labels]

        c_vals = [data["c"][op][l] for l in labels]
        fs_vals = [data["fsharp"][op][l] for l in labels]

        x = np.arange(len(labels))
        width = 0.35

        ax.bar(x - width/2, c_vals, width, label="C (COO)", color=colors["c"], edgecolor="black")

        fs_plot = []
        for v in fs_vals:
            if v is None:
                fs_plot.append(1e-6 if op == "mv" else 0)
            else:
                fs_plot.append(v)

        bars_fs = ax.bar(x + width/2, fs_plot, width, color=colors["fsharp"], edgecolor="black", alpha=0.8)

        for i, (bar, val) in enumerate(zip(bars_fs, fs_vals)):
            if val is None:
                bar.set_hatch('///')
                bar.set_edgecolor('red')
                bar.set_linewidth(2)
                y_max = ax.get_ylim()[1]
                ax.text(x[i] + width/2, y_max * 0.95, "N/A",
                        ha='center', va='top', color='red', fontweight='bold', fontsize=11)

        ax.set_xlabel("Matrix Size (density)")
        ax.set_ylabel("Time (seconds)")
        ax.set_title(f"{titles[op]}")
        ax.set_xticks(x)
        xlabels = [f"{s}×{s}\n(d={d})" for s, d in zip(sizes, densities)]
        ax.set_xticklabels(xlabels, rotation=45, ha="right")
        ax.legend(["C (COO)", "F# (QuadTree)", "F# — not executed (timeout/SO)"])
        ax.grid(axis="y", alpha=0.3, linestyle="--")

        if op == "mv":
            ax.set_yscale("log")
            ax.set_ylabel("Time (seconds, log scale)")

    plt.tight_layout()
    plt.savefig(OUTPUT_PNG, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"Saved: {OUTPUT_PNG}")

def generate_table(data):
    lines = [
        "# Benchmark Results", "",
        "| Size | Density | Operation | C (time) | F# (time) | Note |",
        "|------|---------|-----------|----------|-----------|--------|"
    ]
    for op in ["mm", "mv"]:
        op_name = "mm" if op == "mm" else "mv"
        for label in data["c"][op]:
            size, density = label.split("_")
            c_t = data["c"][op][label]
            f_t = data["fsharp"][op][label]
            c_fmt = format_time(c_t)
            f_fmt = format_time(f_t) if f_t is not None else "N/A"
            note = "OK" if f_t is not None else "Timeout / Stack Overflow"
            lines.append(f"| {size}×{size} | {density} | {op_name} | {c_fmt} | {f_fmt} | {note} |")

    with open(OUTPUT_TABLE, "w") as f:
        f.write("\n".join(lines))
    print(f"Saved: {OUTPUT_TABLE}")

def main():
    print(f"Loading {RESULTS_PATH}...")
    data = load_results(RESULTS_PATH)
    print("Plotting results...")
    plot_results(data)
    print("Generating table...")
    generate_table(data)
    print(f"\nDone. Check files:")
    print(f"   - {OUTPUT_PNG}")
    print(f"   - {OUTPUT_TABLE}")

if __name__ == "__main__":
    main()