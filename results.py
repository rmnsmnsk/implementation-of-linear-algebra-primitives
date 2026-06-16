import matplotlib.pyplot as plt
import sys
import os

my_res = []
cs_res = []

result_file = 'build/benchmark_results.txt'
if not os.path.exists(result_file):
    print(f"File {result_file} not found!")
    print("Please run benchmark first:")
    print("cd build && ./benchmark > benchmark_results.txt")
    sys.exit(1)

try:
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
except Exception as e:
    print(f"Error reading file: {e}")
    sys.exit(1)

if not my_res or not cs_res:
    print("No results found in file")
    sys.exit(1)

if len(my_res) != len(cs_res):
    print(f"Results count mismatch: MY={len(my_res)}, CS={len(cs_res)}")
    sys.exit(1)

names = [r['name'] for r in my_res]
my_times = [r['time'] for r in my_res]
cs_times = [r['time'] for r in cs_res]
my_nnz = [r['nnz'] for r in my_res]

fig, axes = plt.subplots(2, 2, figsize=(14, 10))

ax1 = axes[0, 0]
x = range(len(names))
width = 0.35
ax1.bar([i - width/2 for i in x], my_times, width, label='My COO', color='#3498db')
ax1.bar([i + width/2 for i in x], cs_times, width, label='CSparse', color='#e74c3c')
ax1.set_xticks(list(x))
ax1.set_xticklabels(names, rotation=45, ha='right')
ax1.set_ylabel('Time (ms)')
ax1.set_title('All Results')
ax1.legend()
ax1.grid(axis='y', alpha=0.3)

ax2 = axes[0, 1]
ax2.bar(x, cs_times, width, label='CSparse', color='#e74c3c')
ax2.set_xticks(list(x))
ax2.set_xticklabels(names, rotation=45, ha='right')
ax2.set_ylabel('Time (ms)')
ax2.set_title('CSparse Only')
ax2.legend()
ax2.grid(axis='y', alpha=0.3)

ax3 = axes[1, 0]
ax3.scatter(my_nnz, my_times, s=100, label='My COO', color='#3498db')
ax3.scatter(my_nnz, cs_times, s=100, label='CSparse', color='#e74c3c')
ax3.set_xlabel('NNZ')
ax3.set_ylabel('Time (ms)')
ax3.set_title('Time vs NNZ')
ax3.legend()
ax3.grid(True, alpha=0.3)

ax4 = axes[1, 1]
ax4.scatter(my_nnz, my_times, s=100, label='My COO', color='#3498db')
ax4.scatter(my_nnz, cs_times, s=100, label='CSparse', color='#e74c3c')
ax4.set_xlabel('NNZ')
ax4.set_ylabel('Time (ms)')
ax4.set_title('Time vs NNZ (Log Scale)')
ax4.set_yscale('log')
ax4.legend()
ax4.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('comparison_graph.png', dpi=200, bbox_inches='tight')
plt.show()

print("\n" + "="*80)
print(f"{'Matrix':<20} {'NNZ':>12} {'My Time (ms)':>15} {'CS Time (ms)':>15} {'Speedup':>12}")
print("="*80)
for i in range(len(names)):
    speedup = my_times[i] / cs_times[i] if cs_times[i] > 0 else 0
    print(f"{names[i]:<20} {my_nnz[i]:>12} {my_times[i]:>15.3f} {cs_times[i]:>15.3f} {speedup:>11.2f}x")
print("="*80)