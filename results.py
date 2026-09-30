import matplotlib.pyplot as plt
import sys
import os
import csv
import math

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
                    'standard_deviation': float(parts[4]),
                    'confidence_95': float(parts[5])
                })
        elif line.startswith('RESULT_CS:'):
            parts = line.strip().split(':')[1].split(',')
            if len(parts) >= 4:
                cs_res.append({
                    'name': parts[0],
                    'nnz': int(parts[1]),
                    'time': float(parts[3]),
                    'standard_deviation': float(parts[4]),
                    'confidence_95': float(parts[5])
                })

if not my_res or not cs_res:
    print("No results found")
    sys.exit(1)

student_t_95_df14 = 2.1447866879
sample_groups = {}
current_operation = None
with open(result_file, 'r') as f:
    for line in f:
        if line.startswith('Matrix-Matrix Multiplication:'):
            current_operation = 'matrix'
        elif line.startswith('Matrix-Vector Multiplication:'):
            current_operation = 'vector'
        elif line.startswith('SAMPLES_') and current_operation:
            label, values = line.strip().split(':', 1)
            parts = values.split(',')
            sample_groups[(current_operation, parts[0], label)] = [float(value) for value in parts[1:]]


def sample_stats(samples):
    mean = sum(samples) / len(samples)
    squared_deviations = sum((value - mean) ** 2 for value in samples)
    standard_deviation = math.sqrt(squared_deviations / (len(samples) - 1))
    confidence_95 = student_t_95_df14 * standard_deviation / math.sqrt(len(samples))
    return mean, standard_deviation, confidence_95


analysis_rows = []
for operation in ('matrix', 'vector'):
    for name in sorted({key[1] for key in sample_groups if key[0] == operation}):
        my_samples = sample_groups.get((operation, name, 'SAMPLES_MY'))
        cs_samples = sample_groups.get((operation, name, 'SAMPLES_CS'))
        if my_samples and cs_samples:
            differences = [my - cs for my, cs in zip(my_samples, cs_samples)]
            mean, standard_deviation, confidence_95 = sample_stats(differences)
            analysis_rows.append({
                'operation': operation,
                'matrix': name,
                'comparison': 'coo_minus_csparse',
                'mean_difference_ms': mean,
                'standard_deviation_ms': standard_deviation,
                'confidence_95_ms': confidence_95,
                'confidence_low_ms': mean - confidence_95,
                'confidence_high_ms': mean + confidence_95
            })

        if operation == 'vector':
            baseline_samples = sample_groups.get((operation, name, 'SAMPLES_BASELINE'))
            if baseline_samples and my_samples:
                differences = [baseline - indexed for baseline, indexed in zip(baseline_samples, my_samples)]
                mean, standard_deviation, confidence_95 = sample_stats(differences)
                analysis_rows.append({
                    'operation': operation,
                    'matrix': name,
                    'comparison': 'baseline_minus_indexed',
                    'mean_difference_ms': mean,
                    'standard_deviation_ms': standard_deviation,
                    'confidence_95_ms': confidence_95,
                    'confidence_low_ms': mean - confidence_95,
                    'confidence_high_ms': mean + confidence_95
                })

if analysis_rows:
    with open('benchmark_statistics.csv', 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=analysis_rows[0].keys())
        writer.writeheader()
        writer.writerows(analysis_rows)

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
    mat_my_errors[0].append(my_res[i]['confidence_95'])
    mat_my_errors[1].append(my_res[i]['confidence_95'])
    mat_cs_errors[0].append(cs_res[i]['confidence_95'])
    mat_cs_errors[1].append(cs_res[i]['confidence_95'])
    mat_nnz.append(my_res[i]['nnz'])

    vec_names.append(my_res[i+1]['name'])
    vec_my_times.append(my_res[i+1]['time'])
    vec_cs_times.append(cs_res[i+1]['time'])
    vec_my_errors[0].append(my_res[i+1]['confidence_95'])
    vec_my_errors[1].append(my_res[i+1]['confidence_95'])
    vec_cs_errors[0].append(cs_res[i+1]['confidence_95'])
    vec_cs_errors[1].append(cs_res[i+1]['confidence_95'])
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
                    'standard_deviation': float(parts[4]),
                    'confidence_95': float(parts[5])
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
        vec_my_errors[0][i] = my_result['confidence_95']
        vec_my_errors[1][i] = my_result['confidence_95']
        vec_cs_errors[0][i] = cs_result['confidence_95']
        vec_cs_errors[1][i] = cs_result['confidence_95']

fig, axes = plt.subplots(3, 2, figsize=(18, 24))
fig.suptitle('Mean execution time; error bars show the 95% confidence interval', fontsize=15, y=0.985)

ax1 = axes[0, 0]
x = range(len(mat_names))
width = 0.6
ax1.bar(x, mat_my_times, width, yerr=mat_my_errors, capsize=4, label='COO', color='#3498db')
ax1.set_xticks(x)
ax1.set_xticklabels(mat_names, rotation=25, ha='right', fontsize=10)
ax1.set_ylabel('Mean time (ms)', fontsize=11)
ax1.set_title('Matrix-Matrix Multiplication (COO)', fontsize=12)
ax1.set_yscale('log')
ax1.legend(fontsize=10)
ax1.grid(axis='y', alpha=0.3)

ax2 = axes[0, 1]
ax2.bar(x, mat_cs_times, width, yerr=mat_cs_errors, capsize=4, label='CSparse', color='#e74c3c')
ax2.set_xticks(x)
ax2.set_xticklabels(mat_names, rotation=25, ha='right', fontsize=10)
ax2.set_ylabel('Mean time (ms)', fontsize=11)
ax2.set_title('Matrix-Matrix Multiplication (CSparse)', fontsize=12)
ax2.set_yscale('log')
ax2.legend(fontsize=10)
ax2.grid(axis='y', alpha=0.3)

ax3 = axes[1, 0]
x = range(len(vec_names))
ax3.bar(x, vec_my_times, width, yerr=vec_my_errors, capsize=4, label='COO', color='#3498db')
ax3.set_xticks(x)
ax3.set_xticklabels(vec_names, rotation=25, ha='right', fontsize=10)
ax3.set_ylabel('Mean time (ms)', fontsize=11)
ax3.set_title('Matrix-Vector Multiplication (COO)', fontsize=12)
ax3.set_yscale('log')
ax3.legend(fontsize=10)
ax3.grid(axis='y', alpha=0.3)

ax4 = axes[1, 1]
ax4.bar(x, vec_cs_times, width, yerr=vec_cs_errors, capsize=4, label='CSparse', color='#e74c3c')
ax4.set_xticks(x)
ax4.set_xticklabels(vec_names, rotation=25, ha='right', fontsize=10)
ax4.set_ylabel('Mean time (ms)', fontsize=11)
ax4.set_title('Matrix-Vector Multiplication (CSparse)', fontsize=12)
ax4.set_yscale('log')
ax4.legend(fontsize=10)
ax4.grid(axis='y', alpha=0.3)

ax5 = axes[2, 0]
ax5.scatter(mat_nnz, mat_my_times, s=100, label='COO', color='#3498db')
ax5.scatter(mat_nnz, mat_cs_times, s=100, label='CSparse', color='#e74c3c')
ax5.set_xlabel('NNZ', fontsize=11)
ax5.set_ylabel('Mean time (ms)', fontsize=11)
ax5.set_title('Time vs NNZ (Matrix-Matrix, Log Scale)', fontsize=12)
ax5.set_xscale('log')
ax5.set_yscale('log')
ax5.legend(fontsize=10)
ax5.grid(True, alpha=0.3)

ax6 = axes[2, 1]
ax6.scatter(vec_nnz, vec_my_times, s=100, label='COO', color='#3498db')
ax6.scatter(vec_nnz, vec_cs_times, s=100, label='CSparse', color='#e74c3c')
ax6.set_xlabel('NNZ', fontsize=11)
ax6.set_ylabel('Mean time (ms)', fontsize=11)
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
    baseline = [float(row['baseline_mean_ms']) for row in indexed_rows]
    indexed = [float(row['indexed_mean_ms']) for row in indexed_rows]
    csparse = [float(row['csparse_mean_ms']) for row in indexed_rows]
    baseline_errors = [
        [float(row['baseline_ci95_ms']) for row in indexed_rows],
        [float(row['baseline_ci95_ms']) for row in indexed_rows]
    ]
    indexed_errors = [
        [float(row['indexed_ci95_ms']) for row in indexed_rows],
        [float(row['indexed_ci95_ms']) for row in indexed_rows]
    ]
    csparse_errors = [
        [float(row['csparse_ci95_ms']) for row in indexed_rows],
        [float(row['csparse_ci95_ms']) for row in indexed_rows]
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
    indexed_axes[0].set_ylabel('Mean time (ms)')
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
    indexed_axes[1].set_ylabel('Mean time (ms)')
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
