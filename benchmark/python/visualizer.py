import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter
from typing import List
from .data_parser import BenchmarkGroup, IterationGroup

# Color Palette Constants
TEAL_AVG = '#1da187'
TEAL_MAX = '#45d6b9'
CORAL_AVG = '#d9644d'
CORAL_MAX = '#ff8973'
GOLD_COLOR = '#f0d381'
BLUE_COLOR = '#4ba3e3'


def setup_plot_style():
    """Configures the global matplotlib styling."""
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


def generate_read_benchmark_plots(benchmark_groups: List[BenchmarkGroup], output_dir: str):
    """Generates and saves a 2x2 grid of plots for each read benchmark group."""
    os.makedirs(output_dir, exist_ok=True)
    setup_plot_style()

    for group in benchmark_groups:
        codecs = group.codecs
        baseline = group.baseline

        if not codecs:
            continue

        codec_names = [c.codec for c in codecs]
        uncompressed_size_gb = codecs[0].uncompressed_gb if codecs[0].uncompressed_gb > 0 else (
            baseline.uncompressed_gb if baseline else 0)

        fig, axes = plt.subplots(2, 2, figsize=(24, 16))
        fig.suptitle(f"Read Profile: {group.filename} | Type: {group.dtype}", fontsize=20, weight='bold', color='white',
                     y=0.96)

        ax1, ax2, ax3, ax4 = axes[0, 0], axes[0, 1], axes[1, 0], axes[1, 1]

        # --- Plot 1: In-Memory File Size ---
        comp_sizes = [c.compressed_gb for c in codecs]
        ax1.plot(codec_names, comp_sizes, color=TEAL_AVG, marker='s', linestyle='--', label='Compressed Size (GB)')
        if uncompressed_size_gb > 0:
            ax1.axhline(y=uncompressed_size_gb, color=GOLD_COLOR, linestyle='-', label='Uncompressed Size (GB)')
        max_size_val = max(max(comp_sizes) if comp_sizes else 0, uncompressed_size_gb)
        ax1.set_ylim(bottom=0, top=max_size_val * 1.30)
        for idx, size in enumerate(comp_sizes):
            ax1.text(idx, size + (max_size_val * 0.02), f"{size:.2f}GB", color=TEAL_AVG, ha='center', va='bottom',
                     fontsize=9)
        ax1.set_title("In-Memory Size: Codecs vs Uncompressed", pad=15)
        ax1.set_ylabel("In-Memory Size (GB)")
        ax1.tick_params(axis='x', rotation=45)
        ax1.legend(loc='upper right', frameon=False)
        ax1.grid(True)
        ax1.spines['top'].set_visible(False)
        ax1.spines['right'].set_visible(False)

        # --- Plot 2: Speed Delta vs Compression Ratio ---
        times = [c.real_time for c in codecs]
        c_ratios = [c.compression_ratio for c in codecs]
        baseline_time = baseline.real_time if baseline else 0.0
        deltas = [t - baseline_time for t in times]
        bar_colors = [CORAL_AVG if d > 0 else TEAL_AVG for d in deltas]

        ax2.bar(codec_names, deltas, color=bar_colors, alpha=0.8, label='Time Delta vs Baseline')
        ax2.axhline(y=0, color='white', linestyle='-', linewidth=1, label=f'Baseline ({baseline_time:.1f}ms)')
        y1_max = max((max(deltas)) if deltas else 1, 0.1)
        y1_min = min((min(deltas)) if deltas else 0, 0.0)
        y1_range = abs(y1_min - y1_max)
        y1_max += y1_range * 0.3
        y1_min -= y1_range * 0.1

        ax2.set_ylim(bottom=y1_min, top=y1_max)
        for idx, d in enumerate(deltas):
            v_align = 'bottom' if d > 0 else 'top'
            offset = max(y1_max, abs(y1_min)) * 0.03 * (1 if d > 0 else -1)
            ax2.text(idx, d + offset, f"{d:+.1f}ms", color=bar_colors[idx], ha='center', va=v_align, fontsize=9)

        ax2.set_title("Read Speed Delta vs. Compression Ratio", pad=15)
        ax2.set_ylabel("Time Delta (ms)")
        ax2.tick_params(axis='x', rotation=45)
        ax2.grid(True, axis='y')
        ax2.spines['top'].set_visible(False)

        ax2_right = ax2.twinx()
        ax2_right.plot(codec_names, c_ratios, color=BLUE_COLOR, marker='d', linestyle='-', linewidth=2,
                       label='Compression Ratio (x)')
        ax2_right.set_ylabel("Compression Ratio (x)", color=BLUE_COLOR)

        y2_max = (max(c_ratios) if c_ratios else 1) * 1.3
        y2_min = y2_max * (y1_min / y1_max) if y1_max != 0 else 0
        y2_range = abs(y2_min - y2_max)

        ax2_right.set_ylim(bottom=y2_min, top=y2_max)
        ax2_right.yaxis.set_major_formatter(FuncFormatter(lambda x, pos: f"{x:.1f}" if x >= 0 else ""))
        ax2_right.spines['top'].set_visible(False)

        lines_1, labels_1 = ax2.get_legend_handles_labels()
        lines_2, labels_2 = ax2_right.get_legend_handles_labels()
        ax2.legend(lines_1 + lines_2, labels_1 + labels_2, loc='upper right', frameon=False)
        for idx, cr in enumerate(c_ratios):
            ax2_right.text(idx, cr + (y2_max * 0.02), f"{cr:.1f}x", color=BLUE_COLOR, ha='center', va='bottom',
                           fontsize=9)

        # --- Plot 3: Avg & Max Memory Usage ---
        cpu_avg = [c.cpu_avg_mb for c in codecs]
        cpu_max = [c.cpu_max_mb for c in codecs]
        gpu_avg = [c.gpu_avg_mb for c in codecs]
        gpu_max = [c.gpu_max_mb for c in codecs]

        # --- NEW: Safely get baseline values ---
        # Assuming 'baseline' is your Optional[BenchmarkRun] variable
        baseline_avg = baseline.cpu_avg_mb if baseline else 0
        baseline_max = baseline.cpu_max_mb if baseline else 0

        # --- UPDATED: Include baseline_max in the scaling calculation ---
        max_mem_val = max(
            max(cpu_max) if cpu_max else 0,
            max(gpu_max) if gpu_max else 0,
            baseline_max
        ) or 1

        ax3.plot(codec_names, cpu_max, color=TEAL_MAX, marker='^', linestyle='-', linewidth=1.5, label='RAM Max')
        ax3.plot(codec_names, cpu_avg, color=TEAL_AVG, marker='s', linestyle='--', linewidth=2, label='RAM Avg')
        ax3.plot(codec_names, gpu_max, color=CORAL_MAX, marker='^', linestyle='-', linewidth=1.5, label='VRAM Max')
        ax3.plot(codec_names, gpu_avg, color=CORAL_AVG, marker='x', linestyle='--', linewidth=2, label='VRAM Avg')

        # --- NEW: Plot the baseline dotted lines ---
        if baseline:
            # Using white with a little transparency and dotted/dash-dot styles to differentiate from the main lines
            ax3.axhline(y=baseline_max, color='white', linestyle=':', linewidth=1.5, alpha=0.7,
                        label='Baseline RAM Max')
            ax3.axhline(y=baseline_avg, color='white', linestyle='-.', linewidth=1.5, alpha=0.5,
                        label='Baseline RAM Avg')

        ax3.set_ylim(bottom=-max_mem_val * 0.03, top=max_mem_val * 1.35)

        for idx in range(len(codec_names)):
            ax3.text(idx, cpu_max[idx] + (max_mem_val * 0.025), f"{cpu_max[idx]:.1f}", color=TEAL_MAX, ha='center',
                     va='bottom', fontsize=8)
            ax3.text(idx, cpu_avg[idx] - (max_mem_val * 0.025), f"{cpu_avg[idx]:.1f}", color=TEAL_AVG, ha='center',
                     va='top', fontsize=8)

        ax3.set_title("Memory Allocation Range (MB)", pad=15)
        ax3.set_ylabel("Memory (MB)")
        ax3.tick_params(axis='x', rotation=45)
        # Increased ncol to 3 to accommodate the new baseline legend entries smoothly
        ax3.legend(loc='upper right', frameon=False, fontsize=9, ncol=3)
        ax3.grid(True)
        ax3.spines['top'].set_visible(False)
        ax3.spines['right'].set_visible(False)

        # --- Plot 4: Scatter Plot ---
        cpu_x = [cr for i, cr in enumerate(c_ratios) if "gpu" not in codec_names[i]]
        cpu_y = [t for i, t in enumerate(times) if "gpu" not in codec_names[i]]
        gpu_x = [cr for i, cr in enumerate(c_ratios) if "gpu" in codec_names[i]]
        gpu_y = [t for i, t in enumerate(times) if "gpu" in codec_names[i]]
        if cpu_x:
            ax4.scatter(cpu_x, cpu_y, color=TEAL_AVG, s=80, zorder=5, label='CPU Codec')
        if gpu_x:
            ax4.scatter(gpu_x, gpu_y, color=CORAL_AVG, s=80, zorder=5, label='GPU Codec')
        if baseline_time > 0:
            ax4.axhline(y=baseline_time, color='white', linestyle='--', linewidth=1,
                        label=f'Baseline ({baseline_time:.1f}ms)')
        for idx, txt in enumerate(codec_names):
            ax4.annotate(txt, (c_ratios[idx], times[idx]), textcoords="offset points", xytext=(0, 10), ha='center',
                         fontsize=9, color='#c5c6c8')

        ax4.set_title("Performance (Speed vs Compression)", pad=15)
        ax4.set_xlabel("Compression Ratio (x)")
        ax4.set_ylabel("Read Time (ms)")
        ax4.grid(True)
        ax4.legend(loc='upper right', frameon=False)

        bottom, top = ax4.get_ylim()
        y_padding = top - bottom
        ax4.set_ylim(bottom - (y_padding * 0.10), top + (y_padding * 0.25))

        ax4.spines['top'].set_visible(False)
        ax4.spines['right'].set_visible(False)

        plt.tight_layout(rect=[0, 0, 1, 0.95], pad=3.0)
        safe_filename = group.filename.replace(".", "_")
        output_filepath = os.path.join(output_dir, f"{safe_filename}_{group.dtype}_read_benchmark.png")
        plt.savefig(output_filepath, dpi=150, bbox_inches='tight')
        plt.close(fig)


def generate_iteration_benchmark_plots(iteration_groups: List[IterationGroup], output_dir: str):
    """Generates and saves 1x2 plots comparing iteration methods for a specific codec."""
    os.makedirs(output_dir, exist_ok=True)
    setup_plot_style()

    for group in iteration_groups:
        if not group.runs:
            continue

        methods = [r.method.replace("iter_", "") for r in group.runs]
        real_times = [r.real_time for r in group.runs]
        mem_avgs = [r.mem_avg_mb for r in group.runs]
        mem_maxs = [r.mem_max_mb for r in group.runs]

        fig, axes = plt.subplots(1, 2, figsize=(18, 7))
        fig.suptitle(f"Iteration Profile: {group.filename} | Codec: {group.codec} ({group.dtype})",
                     fontsize=18, weight='bold', color='white', y=1.02)

        ax1, ax2 = axes[0], axes[1]

        # --- Plot 1: Iteration Speed ---
        bars1 = ax1.bar(methods, real_times, color=BLUE_COLOR, alpha=0.8)
        ax1.set_title("Iteration Real Time (ms)", pad=15)
        ax1.set_ylabel("Time (ms)")
        ax1.tick_params(axis='x', rotation=30)
        ax1.grid(True, axis='y')
        ax1.spines['top'].set_visible(False)
        ax1.spines['right'].set_visible(False)

        # Add value labels
        for bar in bars1:
            yval = bar.get_height()
            ax1.text(bar.get_x() + bar.get_width() / 2, yval + (max(real_times) * 0.02), f"{yval:.1f}",
                     ha='center', va='bottom', color=BLUE_COLOR, fontsize=10)

        # --- Plot 2: Memory Footprint ---
        x = np.arange(len(methods))
        width = 0.35

        bars2_max = ax2.bar(x - width / 2, mem_maxs, width, label='Max Memory', color=TEAL_MAX)
        bars2_avg = ax2.bar(x + width / 2, mem_avgs, width, label='Avg Memory', color=TEAL_AVG)

        ax2.set_title("Iteration Memory Footprint (MB)", pad=15)
        ax2.set_ylabel("Memory (MB)")
        ax2.set_xticks(x)
        ax2.set_xticklabels(methods)
        ax2.tick_params(axis='x', rotation=30)
        ax2.legend(frameon=False)
        ax2.grid(True, axis='y')
        ax2.spines['top'].set_visible(False)
        ax2.spines['right'].set_visible(False)

        # Add value labels for Max and Avg
        max_mem_val = max(mem_maxs) if mem_maxs else 1
        for bar in bars2_max:
            yval = bar.get_height()
            ax2.text(bar.get_x() + bar.get_width() / 2, yval + (max_mem_val * 0.02), f"{yval:.1f}",
                     ha='center', va='bottom', color=TEAL_MAX, fontsize=9)
        for bar in bars2_avg:
            yval = bar.get_height()
            ax2.text(bar.get_x() + bar.get_width() / 2, yval + (max_mem_val * 0.02), f"{yval:.1f}",
                     ha='center', va='bottom', color=TEAL_AVG, fontsize=9)

        plt.tight_layout()
        safe_filename = group.filename.replace(".", "_")
        output_filepath = os.path.join(output_dir, f"{safe_filename}_{group.dtype}_{group.codec}_iteration.png")
        plt.savefig(output_filepath, dpi=150, bbox_inches='tight')
        plt.close(fig)
