"""Compare bounded cache hierarchies and ordinary single-level software caches."""

import argparse
import hashlib
import itertools
import json
import platform
from pathlib import Path

from cache_benchmark import CACHE_STRATEGIES
from research.model import (
    DEFAULT_LIMITS,
    MEMORY_COST,
    average_access_cost,
    hierarchy_layouts,
    percent_of_reference,
    software_sizes,
    valid_strategies,
)
from research.reference import min_misses
from research.results import write_results
from research.runner import Runner
from research.workloads import (
    HIERARCHY_PATTERNS,
    SOFTWARE_PATTERNS,
    generate_requests,
    read_requests,
)

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SOURCE_FILES = (
    "cache.h",
    "cacheSystem.h",
    "cacheParser.h",
    "cacheBenchmarks/cache_benchmark_runner.cpp",
    "cacheBenchmarks/cache_benchmark.py",
    "cacheBenchmarks/cache_research.py",
    "cacheBenchmarks/research/model.py",
    "cacheBenchmarks/research/reference.py",
    "cacheBenchmarks/research/results.py",
    "cacheBenchmarks/research/plots.py",
    "cacheBenchmarks/research/runner.py",
    "cacheBenchmarks/research/workloads.py",
) + tuple(f"cache/{path.name}" for path in sorted((PROJECT_ROOT / "cache").glob("*.h")))


def read_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("hierarchy", "software", "all"), default="all")
    parser.add_argument("--runner", type=Path)
    parser.add_argument("--root", type=Path, default=Path("cacheBenchmarks/reports"))
    parser.add_argument("--requests", type=int, default=20000)
    parser.add_argument("--seeds", type=int, nargs="+", default=[1, 2, 3, 4, 5])
    parser.add_argument("--workload-scale", type=int, default=290)
    parser.add_argument("--total", type=int, default=290)
    parser.add_argument("--level-limits", type=int, nargs=3, default=DEFAULT_LIMITS)
    parser.add_argument("--l1-sizes", type=int, nargs="+", default=[1, 2, 4])
    parser.add_argument("--l2-sizes", type=int, nargs="+", default=[16, 32, 48, 64])
    parser.add_argument("--sizes", type=int, nargs="+", help="Single-cache capacities")
    parser.add_argument("--trace", type=Path, help="Software mode: whitespace-separated object IDs")
    parser.add_argument("--generate-only", action="store_true")
    parser.add_argument("--patterns", nargs="+", choices=SOFTWARE_PATTERNS,
                        help="Only run these patterns (default: all for the selected mode)")
    parser.add_argument("--no-plots", action="store_true",
                        help="Save numeric results without requiring matplotlib")
    args = parser.parse_args(argv)
    if args.requests < 1 or args.workload_scale < 2:
        parser.error("requests must be positive; workload-scale must be >= 2")
    if args.requests + 12 * args.workload_scale > 2147483647:
        parser.error("Request count and workload scale exceed the C++ integer range")
    if len(set(args.seeds)) != len(args.seeds):
        parser.error("Seeds must be distinct")
    if args.sizes and (min(args.sizes) < 1 or max(args.sizes) > 2147483647):
        parser.error("Cache capacities must be in 1..2147483647")
    if args.total > 2147483647:
        parser.error("Total capacity exceeds the C++ integer range")
    if args.trace and args.mode != "software":
        parser.error("--trace is supported with --mode software")
    if args.trace and args.patterns:
        parser.error("Use --trace or --patterns, not both")
    if args.patterns and args.mode in ("hierarchy", "all"):
        if any(pattern not in HIERARCHY_PATTERNS for pattern in args.patterns):
            parser.error("popular/navigation are only supported in software mode")
    if not args.generate_only and (args.runner is None or not args.runner.is_file()):
        parser.error("Provide --runner PATH or use --generate-only")
    if args.trace and not args.trace.is_file():
        parser.error("Request file not found")
    args.root = args.root.resolve()
    return args


def experiment_plan(mode, args):
    if mode == "hierarchy":
        layouts = hierarchy_layouts(args.total, args.l1_sizes, args.l2_sizes, args.level_limits)
        patterns = HIERARCHY_PATTERNS
    else:
        capacities = sorted(set(args.sizes or software_sizes(args.workload_scale)))
        layouts = [(capacity,) for capacity in capacities]
        patterns = ("user_trace",) if args.trace else SOFTWARE_PATTERNS
    if args.patterns:
        patterns = tuple(dict.fromkeys(args.patterns))
    return layouts, patterns


def make_metadata(mode, args, layouts, patterns):
    source_hashes = {
        name: hashlib.sha256((PROJECT_ROOT / name).read_bytes()).hexdigest()
        for name in SOURCE_FILES
    }
    return {
        "schema": 2,
        "mode": mode,
        "requests": args.requests,
        "seeds": [0] if args.trace else args.seeds,
        "workload_scale": args.workload_scale,
        "total": args.total if mode == "hierarchy" else None,
        "level_limits": list(args.level_limits) if mode == "hierarchy" else None,
        "layouts": layouts,
        "patterns": patterns,
        "cold_start": True,
        "penalty": None,
        "python": platform.python_version(),
        "source_hashes": source_hashes,
        "trace_hashes": {},
        "plots": not args.no_plots,
    }


def measure(runner, capacities, strategies, requests, reference_misses, mode):
    counters = runner.run(strategies, capacities, requests)
    request_count = len(requests)
    if counters[2] < reference_misses:
        raise RuntimeError("Observed fewer misses than the matching REF cache")
    row = {
        "capacities": "+".join(map(str, capacities)),
        "strategies": "+".join(strategies),
        "requests": request_count,
        "misses": counters[2],
        "misses_per_1000": 1000.0 * counters[2] / request_count,
        "reference_misses_per_1000": 1000.0 * reference_misses / request_count,
    }
    if mode == "hierarchy":
        row["mean_access_cost"] = average_access_cost(counters)
        reference_cost = 1.0 + MEMORY_COST * reference_misses / request_count
        row["percent_of_reference"] = percent_of_reference(row["mean_access_cost"], reference_cost)
    else:
        row["percent_of_reference"] = percent_of_reference(counters[2], reference_misses)
    for level in range(len(capacities)):
        for offset, name in enumerate(("requests", "hits", "misses")):
            row[f"l{level + 1}_{name}"] = counters[3 + 3 * level + offset]
    return row


def run_experiment(mode, args):
    layouts, patterns = experiment_plan(mode, args)
    folder = args.root / mode
    folder.mkdir(parents=True, exist_ok=True)
    metadata = make_metadata(mode, args, layouts, patterns)
    if args.generate_only:
        (folder / "plan.json").write_text(
            json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8"
        )
        return

    # Check plot support before starting the simulations.
    if not args.no_plots:
        try:
            import matplotlib
        except ImportError as error:
            raise RuntimeError("Plots require matplotlib; install it or use --no-plots") from error
        metadata["matplotlib"] = matplotlib.__version__
    metadata["runner_sha256"] = hashlib.sha256(args.runner.read_bytes()).hexdigest()
    external_requests = read_requests(args.trace) if args.trace else None
    if external_requests is not None:
        metadata["requests"] = len(external_requests)
        metadata["input_sha256"] = hashlib.sha256(args.trace.read_bytes()).hexdigest()

    rows = []
    with Runner(args.runner.resolve(), folder / "work") as runner:
        for pattern in patterns:
            print(f"{mode}: {pattern}, {len(layouts)} sizes/layouts", flush=True)
            for seed in metadata["seeds"]:
                requests = external_requests
                if requests is None:
                    requests = generate_requests(pattern, seed, args.requests, args.workload_scale)
                metadata["trace_hashes"][f"{pattern}:{seed}"] = hashlib.sha256(
                    " ".join(map(str, requests)).encode("utf-8")
                ).hexdigest()
                reference_sizes = {sum(capacities) for capacities in layouts}
                references = {size: min_misses(requests, size) for size in reference_sizes}
                for capacities in layouts:
                    for strategies in itertools.product(CACHE_STRATEGIES, repeat=len(capacities)):
                        if not valid_strategies(strategies, capacities):
                            continue
                        measured = measure(
                            runner,
                            capacities,
                            strategies,
                            requests,
                            references[sum(capacities)],
                            mode,
                        )
                        rows.append({"pattern": pattern, "seed": seed, **measured})

    write_results(folder, rows, metadata, plots=not args.no_plots)
    (folder / "manifest.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(f"Results: {folder}", flush=True)


def main(argv=None):
    args = read_arguments(argv)
    modes = ("hierarchy", "software") if args.mode == "all" else (args.mode,)
    for mode in modes:
        run_experiment(mode, args)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, ValueError) as error:
        raise SystemExit(f"Research failed: {error}")
