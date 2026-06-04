import itertools
import json
import re
import os
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

# 1. Load the benchmark JSON data
filepath = "C:/Users/emild/source/repos/compressed-image/bin-int/compressed-image/x64-release/benchmark/benchmark.json"
try:
    with open(filepath, 'r') as f:
        data = json.loads(f.read())
except FileNotFoundError:
    print(f"Could not find file at {filepath}. Please ensure the path is correct.")
    exit(1)

# Ensure the output directory exists
output_dir = "graphs"
os.makedirs(output_dir, exist_ok=True)

# 2. Setup parsing patterns
name_regex = re.compile(r"([A-Za-z0-9_]+)(?:<([^>]+)>)?\/([A-Za-z0-9_.]+)")

KNOWN_DTYPES = ["uint8_t", "uint16_t", "uint32_t", "half", "float"]
file_groups = {}

for b in data.get("benchmarks", []):
    match = name_regex.match(b["name"])
    if not match:
        continue

    op, template_arg, filename = match.groups()
    if not template_arg:
        template_arg = "unknown"

    if op not in ["read_oiio", "read_compressed"]:
        continue

    dtype = "unknown"
    codec = template_arg

    for dt in KNOWN_DTYPES:
        if template_arg.startswith(dt):
            dtype = dt
            codec = template_arg[len(dt):].lstrip('_')
            if not codec:
                codec = "uncompressed"
            break

    if filename not in file_groups:
        file_groups[filename] = {}
    if dtype not in file_groups[filename]:
        file_groups[filename][dtype] = {'baseline': None, 'codecs': []}

    run_data = {
        "codec": codec,
        "real_time": b.get("real_time", 0.0),
        "cpu_avg_mb": b.get("mem_diff_avg_mb", 0.0),
        "cpu_max_mb": b.get("mem_diff_max_mb", 0.0),
        "gpu_avg_mb": b.get("gpu_diff_avg_mb", 0.0),
        "gpu_max_mb": b.get("gpu_diff_max_mb", 0.0),
        "compressed_gb": b.get("compressed_bytes", 0.0) / (1024 ** 3) if b.get("compressed_bytes") else 0.0,
        "uncompressed_gb": b.get("uncompressed_bytes", 0.0) / (1024 ** 3) if b.get("uncompressed_bytes") else 0.0,
        "compression_ratio": b.get("compression_ratio", 1.0)
    }

    if op == "read_oiio":
        file_groups[filename][dtype]['baseline'] = run_data
    else:
        file_groups[filename][dtype]['codecs'].append(run_data)

# --- PLOT STYLING CONFIGURATION ---
plt.style.use('dark_background')
plt.rcParams.update({
    'grid.color': '#2e3033',
    'grid.linestyle': '--',
    'figure.facecolor': '#1a1b1d',
    'axes.facecolor': '#1a1b1d',
    'font.family': 'monospace',
    'text.color': '#c5c6c8',
    'axes.labelcolor': '#8a8d91',
    'xtick.color': '#8a8d91',
    'ytick.color': '#8a8d91'
})

teal_avg = '#1da187'
teal_max = '#45d6b9'
coral_avg = '#d9644d'
coral_max = '#ff8973'
gold_color = '#f0d381'
purple_color = '#a17ecb'
blue_color = '#4ba3e3'

for filename, dtypes_dict in file_groups.items():
    for dtype, group in dtypes_dict.items():
        codecs = group['codecs']
        baseline = group['baseline']

        if not codecs:
            continue

        codec_names = [c['codec'] for c in codecs]

        uncompressed_size_gb = 0
        if codecs and codecs[0]['uncompressed_gb'] > 0:
            uncompressed_size_gb = codecs[0]['uncompressed_gb']
        elif baseline and baseline.get('uncompressed_gb', 0) > 0:
            uncompressed_size_gb = baseline['uncompressed_gb']

        # Create a 2x2 Grid
        fig, axes = plt.subplots(2, 2, figsize=(24, 16))
        fig.suptitle(f"Performance Profile: {filename} | Type: {dtype}", fontsize=20, weight='bold', color='white',
                     y=0.96)

        ax1 = axes[0, 0]  # Top-Left: File Size
        ax2 = axes[0, 1]  # Top-Right: Speed Delta vs Ratio
        ax3 = axes[1, 0]  # Bottom-Left: Memory
        ax4 = axes[1, 1]  # Bottom-Right: Scatter Plot

        # ---------------------------------------------------------
        # Plot 1: In-Memory File Size
        # ---------------------------------------------------------
        comp_sizes = [c['compressed_gb'] for c in codecs]

        ax1.plot(codec_names, comp_sizes, color=teal_avg, marker='s', linestyle='--', label='Compressed Size (GB)')
        if uncompressed_size_gb > 0:
            ax1.axhline(y=uncompressed_size_gb, color=gold_color, linestyle='-', label='Uncompressed Size (GB)')

        max_size_val = max(max(comp_sizes) if comp_sizes else 0, uncompressed_size_gb)
        ax1.set_ylim(bottom=0, top=max_size_val * 1.30)

        for idx, size in enumerate(comp_sizes):
            ax1.text(idx, size + (max_size_val * 0.02), f"{size:.2f}GB", color=teal_avg, ha='center', va='bottom',
                     fontsize=9)

        ax1.set_title("In-Memory Size: Codecs vs Uncompressed", pad=15)
        ax1.set_ylabel("In-Memory Size (GB)")
        ax1.tick_params(axis='x', rotation=45)
        ax1.legend(loc='upper right', frameon=False)
        ax1.grid(True)
        ax1.spines['top'].set_visible(False)
        ax1.spines['right'].set_visible(False)

        # ---------------------------------------------------------
        # Plot 2: Speed Delta vs Compression Ratio (Bar & Line)
        # ---------------------------------------------------------
        times = [c['real_time'] for c in codecs]
        c_ratios = [c['compression_ratio'] for c in codecs]

        baseline_time = baseline['real_time'] if baseline else 0.0
        deltas = [t - baseline_time for t in times]

        bar_colors = [coral_avg if d > 0 else teal_avg for d in deltas]

        ax2.bar(codec_names, deltas, color=bar_colors, alpha=0.8, label='Time Delta vs Baseline')
        ax2.axhline(y=0, color='white', linestyle='-', linewidth=1, label=f'Baseline ({baseline_time:.1f}ms)')

        # Dynamic, asymmetric limits for the left axis
        y1_max = max((max(deltas) * 1.5) if deltas else 1, 0.1)
        y1_min = min((min(deltas) * 1.5) if deltas else 0, 0.0)
        ax2.set_ylim(bottom=y1_min, top=y1_max)

        # Prevent text from colliding on extreme outliers
        for idx, d in enumerate(deltas):
            v_align = 'bottom' if d > 0 else 'top'
            offset = max(y1_max, abs(y1_min)) * 0.03 * (1 if d > 0 else -1)
            ax2.text(idx, d + offset, f"{d:+.1f}ms", color=bar_colors[idx], ha='center', va=v_align, fontsize=9)

        ax2.set_title("Read Speed Delta vs. Compression Ratio", pad=15)
        ax2.set_ylabel("Time Delta (ms)")
        ax2.tick_params(axis='x', rotation=45)
        ax2.grid(True, axis='y')
        ax2.spines['top'].set_visible(False)

        # Secondary Y-Axis for Compression Ratio
        ax2_right = ax2.twinx()
        ax2_right.plot(codec_names, c_ratios, color=blue_color, marker='d', linestyle='-', linewidth=2,
                       label='Compression Ratio (x)')
        ax2_right.set_ylabel("Compression Ratio (x)", color=blue_color)

        # Calculate matching limits so the zeroes on both axes perfectly align
        y2_max = (max(c_ratios) if c_ratios else 1) * 1.35
        y2_min = y2_max * (y1_min / y1_max)  # The mathematical alignment ratio

        ax2_right.set_ylim(bottom=y2_min, top=y2_max)

        # Hide negative compression ratio labels (since they are mathematically forced for alignment, not real data)
        ax2_right.yaxis.set_major_formatter(FuncFormatter(lambda x, pos: f"{x:.1f}" if x >= 0 else ""))
        ax2_right.spines['top'].set_visible(False)

        lines_1, labels_1 = ax2.get_legend_handles_labels()
        lines_2, labels_2 = ax2_right.get_legend_handles_labels()
        ax2.legend(lines_1 + lines_2, labels_1 + labels_2, loc='upper right', frameon=False)

        for idx, cr in enumerate(c_ratios):
            ax2_right.text(idx, cr + (y2_max * 0.02), f"{cr:.1f}x", color=blue_color, ha='center', va='bottom',
                           fontsize=9)

        # ---------------------------------------------------------
        # Plot 3: Avg & Max Memory Usage (MB)
        # ---------------------------------------------------------
        cpu_avg = [c['cpu_avg_mb'] for c in codecs]
        cpu_max = [c['cpu_max_mb'] for c in codecs]
        gpu_avg = [c['gpu_avg_mb'] for c in codecs]
        gpu_max = [c['gpu_max_mb'] for c in codecs]

        max_mem_val = max(max(cpu_max), max(gpu_max)) if max(cpu_max) > 0 or max(gpu_max) > 0 else 1

        ax3.plot(codec_names, cpu_max, color=teal_max, marker='^', linestyle='-', linewidth=1.5, label='RAM Max')
        ax3.plot(codec_names, cpu_avg, color=teal_avg, marker='s', linestyle='--', linewidth=2, label='RAM Avg')
        ax3.plot(codec_names, gpu_max, color=coral_max, marker='^', linestyle='-', linewidth=1.5, label='VRAM Max')
        ax3.plot(codec_names, gpu_avg, color=coral_avg, marker='x', linestyle='--', linewidth=2, label='VRAM Avg')

        ax3.set_ylim(bottom=-max_mem_val * 0.03, top=max_mem_val * 1.35)

        for idx in range(len(codec_names)):
            ax3.text(idx, cpu_max[idx] + (max_mem_val * 0.025), f"{cpu_max[idx]:.1f}", color=teal_max, ha='center',
                     va='bottom', fontsize=8)
            ax3.text(idx, cpu_avg[idx] - (max_mem_val * 0.025), f"{cpu_avg[idx]:.1f}", color=teal_avg, ha='center',
                     va='top', fontsize=8)
            if gpu_max[idx] > 0:
                ax3.text(idx, gpu_max[idx] + (max_mem_val * 0.025), f"{gpu_max[idx]:.1f}", color=coral_max, ha='center',
                         va='bottom', fontsize=8)
            if gpu_avg[idx] > 0:
                ax3.text(idx, gpu_avg[idx] - (max_mem_val * 0.025), f"{gpu_avg[idx]:.1f}", color=coral_avg, ha='center',
                         va='top', fontsize=8)

        ax3.set_title("Memory Allocation Range (MB)", pad=15)
        ax3.set_ylabel("Memory (MB)")
        ax3.tick_params(axis='x', rotation=45)
        ax3.legend(loc='upper right', frameon=False, fontsize=9, ncol=2)
        ax3.grid(True)
        ax3.spines['top'].set_visible(False)
        ax3.spines['right'].set_visible(False)

        # ---------------------------------------------------------
        # Plot 4: Scatter Plot (Time vs Compression Ratio)
        # ---------------------------------------------------------
        # Isolate CPU vs GPU lists
        cpu_x = [cr for i, cr in enumerate(c_ratios) if "gpu" not in codec_names[i]]
        cpu_y = [t for i, t in enumerate(times) if "gpu" not in codec_names[i]]

        gpu_x = [cr for i, cr in enumerate(c_ratios) if "gpu" in codec_names[i]]
        gpu_y = [t for i, t in enumerate(times) if "gpu" in codec_names[i]]

        if cpu_x:
            ax4.scatter(cpu_x, cpu_y, color=teal_avg, s=80, zorder=5, label='CPU Codec')
        if gpu_x:
            ax4.scatter(gpu_x, gpu_y, color=coral_avg, s=80, zorder=5, label='GPU Codec')

        if baseline_time > 0:
            ax4.axhline(y=baseline_time, color='white', linestyle='--', linewidth=1,
                        label=f'Baseline ({baseline_time:.1f}ms)')

        for idx, txt in enumerate(codec_names):
            ax4.annotate(txt, (c_ratios[idx], times[idx]), textcoords="offset points", xytext=(0, 10), ha='center',
                         fontsize=9, color='#c5c6c8')

        ax4.set_title("Performance Frontier (Speed vs Compression)", pad=15)
        ax4.set_xlabel("Compression Ratio (x)")
        ax4.set_ylabel("Read Time (ms)")

        max_scatter_time = max(max(times), baseline_time) if times else 1
        ax4.set_ylim(bottom=0, top=max_scatter_time * 1.25)

        ax4.text(0.95, 0.05, "Better $\\rightarrow$", transform=ax4.transAxes, ha='right', va='bottom', color='white',
                 alpha=0.3, fontsize=12, weight='bold')

        ax4.grid(True)
        ax4.legend(loc='upper right', frameon=False)
        ax4.spines['top'].set_visible(False)
        ax4.spines['right'].set_visible(False)

        # Clean Layout
        plt.tight_layout(rect=[0, 0, 1, 0.95], pad=3.0)

        safe_filename = filename.replace(".", "_")
        output_filepath = os.path.join(output_dir, f"{safe_filename}_{dtype}_benchmark.png")

        plt.savefig(output_filepath, dpi=150, bbox_inches='tight')
        print(f"Saved: {output_filepath}")

        plt.close(fig)

print("All graphs successfully generated!")
