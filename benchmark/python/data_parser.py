import json
import re
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple


# --- DATACLASS DEFINITIONS ---

@dataclass
class BenchmarkRun:
    codec: str
    real_time: float
    cpu_avg_mb: float
    cpu_max_mb: float
    gpu_avg_mb: float
    gpu_max_mb: float
    compressed_gb: float
    uncompressed_gb: float
    compression_ratio: float


@dataclass
class BenchmarkGroup:
    filename: str
    dtype: str
    baseline: Optional[BenchmarkRun] = None
    codecs: List[BenchmarkRun] = field(default_factory=list)


@dataclass
class IterationRun:
    method: str
    real_time: float
    mem_avg_mb: float
    mem_max_mb: float


@dataclass
class IterationGroup:
    filename: str
    dtype: str
    codec: str
    runs: List[IterationRun] = field(default_factory=list)


# --- PARSING LOGIC ---

KNOWN_DTYPES = ["uint8_t", "uint16_t", "uint32_t", "half", "float"]
# Updated regex to stop capturing the filename at the next slash (ignoring trailing parameters)
NAME_REGEX = re.compile(r"([A-Za-z0-9_]+)(?:<([^>]+)>)?\/([^\/]+)")


def load_benchmark_data(filepath: str) -> dict:
    """Loads JSON data from the specified filepath."""
    try:
        with open(filepath, 'r') as f:
            return json.load(f)
    except FileNotFoundError:
        raise FileNotFoundError(f"Could not find file at {filepath}. Please ensure the path is correct.")


def parse_benchmark_data(data: dict) -> Tuple[List[BenchmarkGroup], List[IterationGroup]]:
    """Parses raw benchmark data into structured dataclasses for reads and iterations."""
    read_groups_map: Dict[str, Dict[str, BenchmarkGroup]] = {}
    iter_groups_map: Dict[str, Dict[str, Dict[str, IterationGroup]]] = {}

    for b in data.get("benchmarks", []):
        match = NAME_REGEX.match(b["name"])
        if not match:
            continue

        op, template_arg, filename = match.groups()
        template_arg = template_arg or "unknown"

        dtype = "unknown"
        codec = template_arg

        for dt in KNOWN_DTYPES:
            if template_arg.startswith(dt):
                dtype = dt
                codec = template_arg[len(dt):].lstrip('_')
                if not codec:
                    codec = "uncompressed"
                break

        # Handle Read Operations
        if op in ["read_oiio", "read_compressed"]:
            if filename not in read_groups_map:
                read_groups_map[filename] = {}
            if dtype not in read_groups_map[filename]:
                read_groups_map[filename][dtype] = BenchmarkGroup(filename=filename, dtype=dtype)

            group = read_groups_map[filename][dtype]

            run = BenchmarkRun(
                codec=codec,
                real_time=b.get("real_time", 0.0),
                cpu_avg_mb=b.get("mem_avg_mb", 0.0),
                cpu_max_mb=b.get("mem_max_mb", 0.0),
                gpu_avg_mb=b.get("gpu_avg_mb", 0.0),
                gpu_max_mb=b.get("gpu_max_mb", 0.0),
                compressed_gb=b.get("compressed_bytes", 0.0) / (1024 ** 3) if b.get("compressed_bytes") else 0.0,
                uncompressed_gb=b.get("uncompressed_bytes", 0.0) / (1024 ** 3) if b.get("uncompressed_bytes") else 0.0,
                compression_ratio=b.get("compression_ratio", 1.0)
            )

            if op == "read_oiio":
                group.baseline = run
            else:
                group.codecs.append(run)

        # Handle Iteration Operations
        elif op.startswith("iter_"):
            method = op

            if filename not in iter_groups_map:
                iter_groups_map[filename] = {}
            if dtype not in iter_groups_map[filename]:
                iter_groups_map[filename][dtype] = {}
            if codec not in iter_groups_map[filename][dtype]:
                iter_groups_map[filename][dtype][codec] = IterationGroup(filename=filename, dtype=dtype, codec=codec)

            iter_group = iter_groups_map[filename][dtype][codec]

            iter_run = IterationRun(
                method=method,
                real_time=b.get("real_time", 0.0),
                mem_avg_mb=b.get("mem_avg_mb", 0.0),
                mem_max_mb=b.get("mem_max_mb", 0.0)
            )
            iter_group.runs.append(iter_run)

    # Flatten dictionaries into lists
    read_list = [group for dtype_map in read_groups_map.values() for group in dtype_map.values()]
    iter_list = [
        group
        for dtype_map in iter_groups_map.values()
        for codec_map in dtype_map.values()
        for group in codec_map.values()
    ]

    return read_list, iter_list
