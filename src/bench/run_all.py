import subprocess
import json
import os

RESULTS_PATH = "results.json"
TESTS = [(100, 0.05), (500, 0.02), (1000, 0.01)]
OPS = ["mm", "mv"]

REAL_MATRICES = [
    ("matrices/curtis54.mtx", "curtis54"),
    ("matrices/cage4.mtx", "cage4"),
    ("matrices/lp_afiro.mtx", "lp_afiro"),
]

def run_c_from_file(matrix_path, op):
    cmd = ["./build/benchmark_c", matrix_path, op, "--mtx"]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        return float(proc.stdout.strip())
    except:
        return None

def run_c(size, density, op):
    cmd = ["./build/benchmark_c", str(size), str(density), op]
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    return float(proc.stdout.strip())

def run_fsharp(size, density, op):
    cmd = [
        "dotnet", "run", "--project",
        "../QTreeFSharp/QuadTree/QTreeBench.fsproj",
        "--", str(size), str(density), op
    ]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=3600)
    except subprocess.TimeoutExpired:
        return None

    if proc.returncode != 0 and proc.returncode != 134:
        return None

    lines = [l.strip() for l in proc.stdout.split('\n') if l.strip()]
    for line in reversed(lines):
        try:
            return float(line)
        except ValueError:
            continue
    return None

if os.path.exists(RESULTS_PATH):
    with open(RESULTS_PATH, "r") as f:
        RESULTS = json.load(f)
else:
    RESULTS = {"c": {}, "fsharp": {}}

for op in OPS:
    if op not in RESULTS["c"]: RESULTS["c"][op] = {}
    if op not in RESULTS["fsharp"]: RESULTS["fsharp"][op] = {}

    for size, density in TESTS:
        label = f"{size}_{density}"

        if label not in RESULTS["c"][op]:
            print(f"Running {op} | C | {label}")
            times_c = [run_c(size, density, op) for _ in range(3)]
            RESULTS["c"][op][label] = sum(times_c) / 3

        if op == "mv" and size >= 500:
            RESULTS["fsharp"][op][label] = None
            print(f"Running {op} | F# | {label} -> SKIPPED (Known Stack Overflow in Vector.fs)")
            continue

        if label not in RESULTS["fsharp"][op] or RESULTS["fsharp"][op][label] is None:
            print(f"Running {op} | F# | {label}")
            times_fs = [run_fsharp(size, density, op) for _ in range(3)]
            times_fs = [t for t in times_fs if t is not None]
            if times_fs:
                RESULTS["fsharp"][op][label] = sum(times_fs) / len(times_fs)
                print(f"  C: {RESULTS['c'][op][label]:.6f}s | F#: {RESULTS['fsharp'][op][label]:.6f}s")
            else:
                RESULTS["fsharp"][op][label] = None
                print(f"  C: {RESULTS['c'][op][label]:.6f}s | F#: SKIPPED")

with open(RESULTS_PATH, "w") as f:
    json.dump(RESULTS, f, indent=2)
print("results.json обновлён")