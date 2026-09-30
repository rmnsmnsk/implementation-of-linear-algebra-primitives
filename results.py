import matplotlib.pyplot as plt
import sys
import os
import csv

my_res = []
cs_res = []

result_file = 'benchmark_results_wsl.txt'
if not os.path.exists(result_file):
    print(f"File {result_file} not found!")
    sys.exit(1)

with open(result_file, 'r') as f:
    for line in f:
        if line.startswith('RESULT_MY:'):
            parts = line.strip().split(':')[1].split(',')
            if len(parts) >= 4:
                my_res.append({
                    'name': parts[0],
                    'nnz': int(parts[1]),
                    'time': float(parts[3]),
                    'q1': float(parts[4]) if len(parts) >= 6 else float(parts[3]),
                    'q3': float(parts[5]) if len(parts) >= 6 else float(parts[3])
                })
        elif line.startswith('RESULT_CS:'):
            parts = line.strip().split(':')[1].split(',')
            if len(parts) >= 4:
                cs_res.append({
                    'name': parts[0],
                    'nnz': int(parts[1]),
                    'time': float(parts[3]),
                    'q1': float(parts[4]) if len(parts) >= 6 else float(parts[3]),
                    'q3': float(parts[5]) if len(parts) >= 6 else float(parts[3])
                })

if not my_res or not cs_res:
    print("No results found")
    sys.exit(1)

mat_names = []
vec_names = []
mat_my_times = []
vec_my_times = []
mat_cs_times = []
vec_cs_times = []
mat_my_errors = [[], []]
mat_cs_errors = [[], []]
vec_my_errors = [[], []]
vec_cs_errors = [[], []]
mat_nnz = []
vec_nnz = []

for i in range(0, len(my_res), 2):
    mat_names.append(my_res[i]['name'])
    mat_my_times.append(my_res[i]['time'])
    mat_cs_times.append(cs_res[i]['time'])
    mat_my_errors[0].append(my_res[i]['time'] - my_res[i]['q1'])
    mat_my_errors[1].append(my_res[i]['q3'] - my_res[i]['time'])
    mat_cs_errors[0].append(cs_res[i]['time'] - cs_res[i]['q1'])
    mat_cs_errors[1].append(cs_res[i]['q3'] - cs_res[i]['time'])
    mat_nnz.append(my_res[i]['nnz'])

    vec_names.append(my_res[i+1]['name'])
    vec_my_times.append(my_res[i+1]['time'])
    vec_cs_times.append(cs_res[i+1]['time'])
    vec_my_errors[0].append(my_res[i+1]['time'] - my_res[i+1]['q1'])
    vec_my_errors[1].append(my_res[i+1]['q3'] - my_res[i+1]['time'])
    vec_cs_errors[0].append(cs_res[i+1]['time'] - cs_res[i+1]['q1'])
    vec_cs_errors[1].append(cs_res[i+1]['q3'] - cs_res[i+1]['time'])
    vec_nnz.append(my_res[i+1]['nnz'])

indexed_vector_file = 'benchmark_vector_indexed_wsl.txt'
if os.path.exists(indexed_vector_file):
    indexed_my = {}
    indexed_cs = {}
    with open(indexed_vector_file, 'r') as f:
        for line in f:
            if line.startswith('RESULT_MY:') or line.startswith('RESULT_CS:'):
                prefix, values = line.strip().split(':', 1)
                parts = values.split(',')
                if len(parts) < 6:
                    continue
                result = {
                    'nnz': int(parts[1]),
                    'time': float(parts[3]),
                    'q1': float(parts[4]),
                    'q3': float(parts[5])
                }
                if prefix == 'RESULT_MY':
                    indexed_my[parts[0]] = result
                else:
                    indexed_cs[parts[0]] = result

    for i, name in enumerate(vec_names):
        if name not in indexed_my or name not in indexed_cs:
            continue
        my_result = indexed_my[name]
        cs_result = indexed_cs[name]
        vec_nnz[i] = my_result['nnz']
        vec_my_times[i] = my_result['time']
        vec_cs_times[i] = cs_result['time']
        vec_my_errors[0][i] = my_result['time'] - my_result['q1']
        vec_my_errors[1][i] = my_result['q3'] - my_result['time']
        vec_cs_errors[0][i] = cs_result['time'] - cs_result['q1']
        vec_cs_errors[1][i] = cs_result['q3'] - cs_result['time']

fig, axes = plt.subplots(3, 2, figsize=(18, 24))
fig.suptitle('Median execution time; error bars show the Q1-Q3 interval', fontsize=15, y=0.985)

ax1 = axes[0, 0]
x = range(len(mat_names))
width = 0.6
ax1.bar(x, mat_my_times, width, yerr=mat_my_errors, capsize=4, label='COO', color='#3498db')
ax1.set_xticks(x)
ax1.set_xticklabels(mat_names, rotation=25, ha='right', fontsize=10)
ax1.set_ylabel('Median time (ms)', fontsize=11)
ax1.set_title('Matrix-Matrix Multiplication (COO)', fontsize=12)
ax1.set_yscale('log')
ax1.legend(fontsize=10)
ax1.grid(axis='y', alpha=0.3)

ax2 = axes[0, 1]
ax2.bar(x, mat_cs_times, width, yerr=mat_cs_errors, capsize=4, label='CSparse', color='#e74c3c')
ax2.set_xticks(x)
ax2.set_xticklabels(mat_names, rotation=25, ha='right', fontsize=10)
ax2.set_ylabel('Median time (ms)', fontsize=11)
ax2.set_title('Matrix-Matrix Multiplication (CSparse)', fontsize=12)
ax2.set_yscale('log')
ax2.legend(fontsize=10)
ax2.grid(axis='y', alpha=0.3)

ax3 = axes[1, 0]
x = range(len(vec_names))
ax3.bar(x, vec_my_times, width, yerr=vec_my_errors, capsize=4, label='COO', color='#3498db')
ax3.set_xticks(x)
ax3.set_xticklabels(vec_names, rotation=25, ha='right', fontsize=10)
ax3.set_ylabel('Median time (ms)', fontsize=11)
ax3.set_title('Matrix-Vector Multiplication (COO)', fontsize=12)
ax3.set_yscale('log')
ax3.legend(fontsize=10)
ax3.grid(axis='y', alpha=0.3)

ax4 = axes[1, 1]
ax4.bar(x, vec_cs_times, width, yerr=vec_cs_errors, capsize=4, label='CSparse', color='#e74c3c')
ax4.set_xticks(x)
ax4.set_xticklabels(vec_names, rotation=25, ha='right', fontsize=10)
ax4.set_ylabel('Median time (ms)', fontsize=11)
ax4.set_title('Matrix-Vector Multiplication (CSparse)', fontsize=12)
ax4.set_yscale('log')
ax4.legend(fontsize=10)
ax4.grid(axis='y', alpha=0.3)

ax5 = axes[2, 0]
ax5.scatter(mat_nnz, mat_my_times, s=100, label='COO', color='#3498db')
ax5.scatter(mat_nnz, mat_cs_times, s=100, label='CSparse', color='#e74c3c')
ax5.set_xlabel('NNZ', fontsize=11)
ax5.set_ylabel('Median time (ms)', fontsize=11)
ax5.set_title('Time vs NNZ (Matrix-Matrix, Log Scale)', fontsize=12)
ax5.set_xscale('log')
ax5.set_yscale('log')
ax5.legend(fontsize=10)
ax5.grid(True, alpha=0.3)

ax6 = axes[2, 1]
ax6.scatter(vec_nnz, vec_my_times, s=100, label='COO', color='#3498db')
ax6.scatter(vec_nnz, vec_cs_times, s=100, label='CSparse', color='#e74c3c')
ax6.set_xlabel('NNZ', fontsize=11)
ax6.set_ylabel('Median time (ms)', fontsize=11)
ax6.set_title('Time vs NNZ (Matrix-Vector, Log Scale)', fontsize=12)
ax6.set_xscale('log')
ax6.set_yscale('log')
ax6.legend(fontsize=10)
ax6.grid(True, alpha=0.3)

plt.subplots_adjust(hspace=0.8, wspace=0.35, top=0.95, bottom=0.05, left=0.08, right=0.95)
fig.savefig('comparison_graph.png', dpi=300, bbox_inches='tight')

profile_file = 'profiling_results.csv'
if os.path.exists(profile_file):
    profile_rows = []
    with open(profile_file, 'r', newline='') as f:
        for row in csv.DictReader(f):
            total = float(row['total_ms'])
            sort = float(row['sort_ms'])
            accumulation = float(row['accumulation_ms'])
            buffer_scan = float(row['buffer_scan_ms'])
            profile_rows.append({
                'operation': row['operation'],
                'name': row['matrix'],
                'total': total,
                'sort': sort,
                'accumulation': accumulation,
                'buffer_scan': buffer_scan,
                'other': total - sort - accumulation - buffer_scan
            })

    matrix_profile = [row for row in profile_rows if row['operation'] == 'matrix']
    vector_profile = [row for row in profile_rows if row['operation'] == 'vector']
    profile_fig, profile_axes = plt.subplots(1, 2, figsize=(16, 7))
    profile_fig.suptitle('COO profiling used to select optimizations: share of total execution time', fontsize=15, y=0.98)

    def plot_profile(ax, rows, title, accumulation_label, include_buffer):
        names = [row['name'] for row in rows]
        totals = [row['total'] for row in rows]
        sort_share = [100.0 * row['sort'] / row['total'] for row in rows]
        accumulation_share = [100.0 * row['accumulation'] / row['total'] for row in rows]
        buffer_share = [100.0 * row['buffer_scan'] / row['total'] for row in rows]
        other_share = [100.0 * row['other'] / row['total'] for row in rows]
        x = range(len(rows))

        ax.bar(x, sort_share, label='Input order check', color='#85c1e9')
        ax.bar(x, accumulation_share, bottom=sort_share,
               label=accumulation_label, color='#3498db')
        bottom = [sort_share[i] + accumulation_share[i] for i in x]
        if include_buffer:
            ax.bar(x, buffer_share, bottom=bottom,
                   label='Touched-word processing', color='#e74c3c')
            bottom = [bottom[i] + buffer_share[i] for i in x]
        ax.bar(x, other_share, bottom=bottom, label='Other phases', color='#95a5a6')
        ax.set_xticks(x)
        ax.set_xticklabels(names, rotation=20, ha='right')
        ax.set_ylim(0, 100)
        ax.set_ylabel('Share of total time (%)')
        ax.set_title(title)
        ax.grid(axis='y', alpha=0.3)
        ax.legend()

    plot_profile(
        profile_axes[0], matrix_profile, 'Matrix-Matrix Multiplication',
        'Product accumulation', True)
    plot_profile(
        profile_axes[1], vector_profile, 'Matrix-Vector Multiplication',
        'Matrix scan, hash lookup and accumulation', False)
    profile_fig.subplots_adjust(wspace=0.25, top=0.88, bottom=0.17)
    profile_fig.savefig('profiling_graph.png', dpi=300, bbox_inches='tight')

indexed_file = 'indexed_vector_results.csv'
if os.path.exists(indexed_file):
    with open(indexed_file, 'r', newline='') as f:
        indexed_rows = list(csv.DictReader(f))

    names = [row['matrix'] for row in indexed_rows]
    baseline = [float(row['baseline_median_ms']) for row in indexed_rows]
    indexed = [float(row['indexed_median_ms']) for row in indexed_rows]
    csparse = [float(row['csparse_median_ms']) for row in indexed_rows]
    baseline_errors = [
        [baseline[i] - float(row['baseline_q1_ms']) for i, row in enumerate(indexed_rows)],
        [float(row['baseline_q3_ms']) - baseline[i] for i, row in enumerate(indexed_rows)]
    ]
    indexed_errors = [
        [indexed[i] - float(row['indexed_q1_ms']) for i, row in enumerate(indexed_rows)],
        [float(row['indexed_q3_ms']) - indexed[i] for i, row in enumerate(indexed_rows)]
    ]
    csparse_errors = [
        [csparse[i] - float(row['csparse_q1_ms']) for i, row in enumerate(indexed_rows)],
        [float(row['csparse_q3_ms']) - csparse[i] for i, row in enumerate(indexed_rows)]
    ]

    indexed_fig, indexed_axes = plt.subplots(1, 2, figsize=(15, 6))
    indexed_fig.suptitle('Sparse matrix-vector multiplication on large matrices', fontsize=15)
    positions = list(range(len(names)))
    bar_width = 0.36

    indexed_axes[0].bar(
        [x - bar_width / 2 for x in positions], baseline, bar_width,
        yerr=baseline_errors, capsize=4, label='Baseline COO', color='#95a5a6')
    indexed_axes[0].bar(
        [x + bar_width / 2 for x in positions], indexed, bar_width,
        yerr=indexed_errors, capsize=4, label='Indexed COO', color='#3498db')
    indexed_axes[0].set_xticks(positions)
    indexed_axes[0].set_xticklabels(names)
    indexed_axes[0].set_ylabel('Median time (ms)')
    indexed_axes[0].set_title('Effect of the optimization')
    indexed_axes[0].grid(axis='y', alpha=0.3)
    indexed_axes[0].legend()

    indexed_axes[1].bar(
        [x - bar_width / 2 for x in positions], indexed, bar_width,
        yerr=indexed_errors, capsize=4, label='Indexed COO', color='#3498db')
    indexed_axes[1].bar(
        [x + bar_width / 2 for x in positions], csparse, bar_width,
        yerr=csparse_errors, capsize=4, label='CSparse', color='#e74c3c')
    indexed_axes[1].set_xticks(positions)
    indexed_axes[1].set_xticklabels(names)
    indexed_axes[1].set_ylabel('Median time (ms)')
    indexed_axes[1].set_title('Final implementation and CSparse')
    indexed_axes[1].grid(axis='y', alpha=0.3)
    indexed_axes[1].legend()

    indexed_fig.subplots_adjust(wspace=0.25, top=0.85, bottom=0.12)
    indexed_fig.savefig('indexed_vector_comparison.png', dpi=300, bbox_inches='tight')

if '--show' in sys.argv:
    plt.show()

print("\n" + "="*80)
print("MATRIX-MATRIX MULTIPLICATION")
print("="*80)
print(f"{'Matrix':<20} {'NNZ':>12} {'COO (ms)':>15} {'CSparse (ms)':>15} {'Speedup':>12}")
print("="*80)
for i in range(len(mat_names)):
    speedup = mat_my_times[i] / mat_cs_times[i] if mat_cs_times[i] > 0 else 0
    print(f"{mat_names[i]:<20} {mat_nnz[i]:>12} {mat_my_times[i]:>15.3f} {mat_cs_times[i]:>15.3f} {speedup:>11.2f}x")
print("="*80)

print("\n" + "="*80)
print("MATRIX-VECTOR MULTIPLICATION")
print("="*80)
print(f"{'Matrix':<20} {'NNZ':>12} {'COO (ms)':>15} {'CSparse (ms)':>15} {'Speedup':>12}")
print("="*80)
for i in range(len(vec_names)):
    speedup = vec_my_times[i] / vec_cs_times[i] if vec_cs_times[i] > 0 else 0
    print(f"{vec_names[i]:<20} {vec_nnz[i]:>12} {vec_my_times[i]:>15.3f} {vec_cs_times[i]:>15.3f} {speedup:>11.2f}x")
print("="*80)
