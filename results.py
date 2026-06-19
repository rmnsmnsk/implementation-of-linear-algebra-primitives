import matplotlib.pyplot as plt
import sys
import os

my_res = []
cs_res = []

result_file = 'build/benchmark_results.txt'
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
                    'time': float(parts[3])
                })
        elif line.startswith('RESULT_CS:'):
            parts = line.strip().split(':')[1].split(',')
            if len(parts) >= 4:
                cs_res.append({
                    'name': parts[0],
                    'nnz': int(parts[1]),
                    'time': float(parts[3])
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
mat_nnz = []
vec_nnz = []

for i in range(0, len(my_res), 2):
    mat_names.append(my_res[i]['name'])
    mat_my_times.append(my_res[i]['time'])
    mat_cs_times.append(cs_res[i]['time'])
    mat_nnz.append(my_res[i]['nnz'])

    vec_names.append(my_res[i+1]['name'])
    vec_my_times.append(my_res[i+1]['time'])
    vec_cs_times.append(cs_res[i+1]['time'])
    vec_nnz.append(my_res[i+1]['nnz'])

fig, axes = plt.subplots(3, 2, figsize=(16, 24))

ax1 = axes[0, 0]
x = range(len(mat_names))
width = 0.6
ax1.bar(x, mat_my_times, width, label='COO', color='#3498db')
ax1.set_xticks(x)
ax1.set_xticklabels(mat_names, rotation=25, ha='right', fontsize=10)
ax1.set_ylabel('Time (ms)', fontsize=11)
ax1.set_title('Matrix-Matrix Multiplication (COO)', fontsize=12)
ax1.legend(fontsize=10)
ax1.grid(axis='y', alpha=0.3)

ax2 = axes[0, 1]
ax2.bar(x, mat_cs_times, width, label='CSparse', color='#e74c3c')
ax2.set_xticks(x)
ax2.set_xticklabels(mat_names, rotation=25, ha='right', fontsize=10)
ax2.set_ylabel('Time (ms)', fontsize=11)
ax2.set_title('Matrix-Matrix Multiplication (CSparse)', fontsize=12)
ax2.legend(fontsize=10)
ax2.grid(axis='y', alpha=0.3)

ax3 = axes[1, 0]
x = range(len(vec_names))
ax3.bar(x, vec_my_times, width, label='COO', color='#3498db')
ax3.set_xticks(x)
ax3.set_xticklabels(vec_names, rotation=25, ha='right', fontsize=10)
ax3.set_ylabel('Time (ms)', fontsize=11)
ax3.set_title('Matrix-Vector Multiplication (COO)', fontsize=12)
ax3.legend(fontsize=10)
ax3.grid(axis='y', alpha=0.3)

ax4 = axes[1, 1]
ax4.bar(x, vec_cs_times, width, label='CSparse', color='#e74c3c')
ax4.set_xticks(x)
ax4.set_xticklabels(vec_names, rotation=25, ha='right', fontsize=10)
ax4.set_ylabel('Time (ms)', fontsize=11)
ax4.set_title('Matrix-Vector Multiplication (CSparse)', fontsize=12)
ax4.legend(fontsize=10)
ax4.grid(axis='y', alpha=0.3)

ax5 = axes[2, 0]
ax5.scatter(mat_nnz, mat_my_times, s=100, label='COO', color='#3498db')
ax5.scatter(mat_nnz, mat_cs_times, s=100, label='CSparse', color='#e74c3c')
ax5.set_xlabel('NNZ', fontsize=11)
ax5.set_ylabel('Time (ms)', fontsize=11)
ax5.set_title('Time vs NNZ (Matrix-Matrix, Log Scale)', fontsize=12)
ax5.set_yscale('log')
ax5.legend(fontsize=10)
ax5.grid(True, alpha=0.3)

ax6 = axes[2, 1]
ax6.scatter(vec_nnz, vec_my_times, s=100, label='COO', color='#3498db')
ax6.scatter(vec_nnz, vec_cs_times, s=100, label='CSparse', color='#e74c3c')
ax6.set_xlabel('NNZ', fontsize=11)
ax6.set_ylabel('Time (ms)', fontsize=11)
ax6.set_title('Time vs NNZ (Matrix-Vector, Log Scale)', fontsize=12)
ax6.set_yscale('log')
ax6.legend(fontsize=10)
ax6.grid(True, alpha=0.3)

plt.subplots_adjust(hspace=0.8, wspace=0.35, top=0.97, bottom=0.05, left=0.08, right=0.95)
plt.savefig('comparison_graph.png', dpi=300, bbox_inches='tight')
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