import json
import matplotlib.pyplot as plt
import numpy as np
import os

RESULTS_PATH = "results.json"
OUTPUT_PNG = "benchmark_results.png"
OUTPUT_TABLE = "results_table.md"

def load_results(path):
    if not os.path.exists(path):
        raise FileNotFoundError(f"File {path} not found")
    with open(path, "r") as f:
        return json.load(f)

def format_time(seconds):
    if seconds is None: return "N/A"
    if seconds < 1e-6: return f"{seconds*1e9:.1f} ns"
    if seconds < 1e-3: return f"{seconds*1e6:.1f} µs"
    if seconds < 1: return f"{seconds*1e3:.1f} ms"
    return f"{seconds:.2f} s"

def is_synthetic(label):
    try:
        int(label.split("_")[0])
        return True
    except (ValueError, IndexError):
        return False

def get_synthetic_size(label):
    try:
        return int(label.split("_")[0])
    except:
        return 0

def plot_results(data):
    ops = ["mm", "mv"]
    titles = {"mm": "Matrix × Matrix", "mv": "Matrix × Vector"}
    colors = {"c": "#2E86AB", "fsharp": "#A23B72"}

    fig, axes = plt.subplots(2, 2, figsize=(16, 10))

    for op_idx, op in enumerate(ops):
        all_labels = list(set(list(data["c"][op].keys()) + list(data["fsharp"][op].keys())))

        fast_labels = []
        slow_labels = []

        for l in all_labels:
            fsharp_val = data["fsharp"][op].get(l)
            if fsharp_val is None:
                slow_labels.append(l)
            elif is_synthetic(l):
                if get_synthetic_size(l) <= 500:
                    fast_labels.append(l)
                else:
                    slow_labels.append(l)
            else:
                fast_labels.append(l)

        fast_labels = sorted(fast_labels, key=lambda x: (0 if is_synthetic(x) else 1, get_synthetic_size(x) if is_synthetic(x) else 0, x))
        slow_labels = sorted(slow_labels, key=lambda x: (0 if is_synthetic(x) else 1, get_synthetic_size(x) if is_synthetic(x) else 0, x))

        for col, (labels, title_suffix, use_log) in enumerate([
            (fast_labels, "Fast", True),
            (slow_labels, "Slow", False if op == "mm" else True)
        ]):
            if not labels:
                axes[op_idx, col].axis('off')
                axes[op_idx, col].set_title(f"{titles[op]} - {title_suffix} (no data)")
                continue

            ax = axes[op_idx, col]
            x = np.arange(len(labels))
            width = 0.35

            c_vals = [data["c"][op].get(l, 0) for l in labels]
            fs_vals = [data["fsharp"][op].get(l) for l in labels]
            fs_plot_vals = [v if v is not None else (1e-10 if op == "mv" else 1e-6) for v in fs_vals]

            ax.bar(x - width/2, c_vals, width, label="C (COO)", color=colors["c"],
                   edgecolor="black", zorder=3)

            bars_fs = ax.bar(x + width/2, fs_plot_vals, width, color=colors["fsharp"],
                            edgecolor="black", alpha=0.8, zorder=3)

            for i, (bar, val) in enumerate(zip(bars_fs, fs_vals)):
                if val is None:
                    bar.set_hatch('///')
                    bar.set_edgecolor('red')
                    bar.set_linewidth(2)
                    y_max = ax.get_ylim()[1]
                    ax.text(x[i] + width/2, y_max * 0.9, "N/A",
                           ha='center', va='top', color='red', fontweight='bold', fontsize=9)

            x_labels = []
            for l in labels:
                if is_synthetic(l):
                    size, density = l.split("_")
                    x_labels.append(f"{size}×{size}\n(d={density})")
                else:
                    x_labels.append(l)

            ax.set_xticks(x)
            ax.set_xticklabels(x_labels, rotation=45, ha="right", fontsize=8)
            ax.set_ylabel("Time (seconds)")
            ax.set_title(f"{titles[op]} - {title_suffix}")
            ax.grid(axis="y", alpha=0.3, linestyle="--")

            if use_log:
                ax.set_yscale("log")
                ax.set_ylabel("Time (seconds, log scale)")

            if op_idx == 0 and col == 0:
                ax.legend(["C (COO)", "F# (QuadTree)", "F# — N/A"], loc="upper left", fontsize=9)

    plt.tight_layout()
    plt.savefig(OUTPUT_PNG, dpi=300, bbox_inches="tight")
    plt.close()
    print(f"✓ Saved: {OUTPUT_PNG}")

def generate_table(data):
    lines = ["# Benchmark Results", "",
             "| Dataset | Operation | C (time) | F# (time) | Note |",
             "|---------|-----------|----------|-----------|--------|"]

    for op in ["mm", "mv"]:
        all_labels = list(set(list(data["c"][op].keys()) + list(data["fsharp"][op].keys())))
        non_synth = sorted([l for l in all_labels if not is_synthetic(l)])
        synth = sorted([l for l in all_labels if is_synthetic(l)], key=lambda x: get_synthetic_size(x))
        final_labels = non_synth + synth

        for label in final_labels:
            dataset_name = label if not is_synthetic(label) else f"Synthetic {label.split('_')[0]}×{label.split('_')[0]}"
            c_t = data["c"][op].get(label)
            f_t = data["fsharp"][op].get(label)
            c_fmt = format_time(c_t) if c_t is not None else "N/A"
            f_fmt = format_time(f_t) if f_t is not None else "N/A"
            note = "OK" if f_t is not None else "Timeout / Stack Overflow"
            lines.append(f"| {dataset_name} | {op} | {c_fmt} | {f_fmt} | {note} |")

    with open(OUTPUT_TABLE, "w") as f:
        f.write("\n".join(lines))
    print(f"✓ Saved: {OUTPUT_TABLE}")

def main():
    print(f"Loading {RESULTS_PATH}...")
    data = load_results(RESULTS_PATH)
    print("Plotting results (Fast vs Slow, N/A → Slow)...")
    plot_results(data)
    print("Generating table...")
    generate_table(data)
    print(f"\nDone! Check:\n   - {OUTPUT_PNG}\n   - {OUTPUT_TABLE}")

if __name__ == "__main__":
    main()