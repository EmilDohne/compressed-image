import logging
import argparse
from python.data_parser import load_benchmark_data, parse_benchmark_data
from python.visualizer import generate_read_benchmark_plots, generate_iteration_benchmark_plots

logger = logging.getLogger(__name__)


def main():
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S"
    )

    # Set up command-line argument parsing
    parser = argparse.ArgumentParser(description="Parse benchmark JSON and generate performance graphs.")
    parser.add_argument(
        "-i", "--input",
        required=True,
        help="Path to the input benchmark JSON file."
    )
    parser.add_argument(
        "-o", "--output",
        default="graphs",
        help="Directory to save the generated graphs (defaults to 'graphs')."
    )

    args = parser.parse_args()

    logger.info(f"Loading benchmark data from {args.input}...")
    raw_data = load_benchmark_data(args.input)

    logger.info("Parsing data into dataclass structures...")
    read_groups, iter_groups = parse_benchmark_data(raw_data)

    logger.info(f"Generating Read Benchmark graphs in '{args.output}'...")
    generate_read_benchmark_plots(read_groups, args.output)

    logger.info(f"Generating Iteration Benchmark graphs in '{args.output}'...")
    generate_iteration_benchmark_plots(iter_groups, args.output)

    logger.info("All graphs successfully generated!")


if __name__ == "__main__":
    main()
