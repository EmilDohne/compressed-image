import sys
import pathlib
import subprocess
import argparse
import logging
from typing import List


logger = logging.getLogger(__name__)


def communicate_and_raise(command: List[str]) -> None:
    logger.info(f"Running command: {' '.join(command)}")
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    # Stream output line by line
    for line in proc.stdout:
        print(line, end="")  # already includes newline

    proc.wait()

    if proc.returncode != 0:
        raise RuntimeError(f"Command failed with exit code {proc.returncode}: {' '.join(command)}")
    

def build_and_repair(
        source_path: pathlib.Path, 
        wheel_path: pathlib.Path, 
        build_subdir: pathlib.Path, 
        verbose: bool
    ) -> None:
    # Step 1: Build the wheel
    wheel_command = [sys.executable, "-m", "pip", "wheel", str(source_path), f"--wheel-dir={str(wheel_path)}"]
    if verbose:
        wheel_command.append("--verbose")
    
    logging.info(f"Building wheel for directory {source_path.absolute()}")
    communicate_and_raise(wheel_command)

     # Step 2: Automatically detect lib directories containing shared libraries
    def is_lib_dir(path: pathlib.Path) -> bool:
        return any(path.glob("*.so")) or any(path.glob("*.dylib")) or any(path.glob("*.dll"))

    lib_dirs = [
        subdir for subdir in build_subdir.rglob("*")
        if subdir.is_dir() and is_lib_dir(subdir)
    ]

    if verbose:
        logging.info("Detected library directories:")
        for d in lib_dirs:
            logging.info(f"  {d}")

    # Step 3: Find all wheels in /build (recursively)
    all_wheels = [
        wheel for wheel in wheel_path.rglob("*.whl")
        if "repaired" not in wheel.parts 
        and ".venv" not in wheel.parts
    ]
    if not all_wheels:
        raise RuntimeError(f"No wheels found in {wheel_path} to repair.")

    # Step 4: Repair each wheel
    repaired_dir = wheel_path / "repaired"
    repaired_dir.mkdir(parents=True, exist_ok=True)

    for wheel in all_wheels:
        logging.info(f"Repairing wheel: {wheel}")
        repair_command = [
            sys.executable, "-m", "repairwheel", str(wheel),
            f"--output-dir={str(repaired_dir)}"
        ]

        for lib_dir in lib_dirs:
            repair_command.extend(["--lib-dir", str(lib_dir)])

        communicate_and_raise(repair_command)


def parse_args():
    parser = argparse.ArgumentParser(description="Build and repair a Python wheel.")
    parser.add_argument(
        "source", type=pathlib.Path,
        help="Path to the source directory or setup.py"
    )
    parser.add_argument(
        "--wheel-dir", type=pathlib.Path, default=pathlib.Path("wheels"),
        help="Directory to place built wheels (default: wheels/)"
    )
    parser.add_argument(
        "--lib-search-dir", type=pathlib.Path, default=pathlib.Path("build"),
        help="Directory under which to search for shared libraries (default: build/)"
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true",
        help="Enable verbose output"
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    build_and_repair(
        source_path=args.source,
        wheel_path=args.wheel_dir,
        build_subdir=args.lib_search_dir,
        verbose=args.verbose
    )