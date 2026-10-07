"""Generate one reproducible benchmark case or run a previously saved case."""

import argparse
import hashlib
import json
import platform
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

from cache_benchmark import (
    REQUEST_PATTERNS,
    check_statistics,
    generate_request_sequence,
)
from research.workloads import SOFTWARE_PATTERNS, generate_requests, pattern_label

ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = ROOT.parent
FILES = ("requests.txt", "input.txt", "config.txt", "manifest.json")
LEGACY_LABELS = {
    "small_cycle": "Цикл 32 страниц",
    "boundary_cycle": "Цикл 65 страниц",
    "scan": "Последовательный цикл 256 страниц",
    "hot_cold": "90% запросов к 8 горячим страницам",
    "hot_scan": "Горячие страницы и сканирование",
    "phase_change": "Смена горячего множества каждые 2000 запросов",
    "bursts": "Серии по 16 одинаковых запросов",
    "uniform": "Равномерные запросы к 256 страницам",
}


def sha256(content):
    return hashlib.sha256(content).hexdigest()


def make_parser():
    parser = argparse.ArgumentParser(
        description=__doc__,
        epilog="Start with 'patterns', then 'generate --pattern hot_cold', then 'run CASE'.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    commands = parser.add_subparsers(dest="command", required=True)

    def command(name, help_text):
        return commands.add_parser(name, help=help_text, description=help_text,
                                   formatter_class=argparse.ArgumentDefaultsHelpFormatter)

    patterns = command("patterns", "List patterns and their meanings")
    patterns.add_argument("--suite", choices=("legacy", "research"), default="research",
                          help="Original fixed-capacity benchmark or newer studies")
    patterns.add_argument("--workload-scale", type=int, default=290, help="Research workload size")

    generate = command("generate", "Generate one case; no runner required")
    generate.add_argument("--suite", choices=("legacy", "research"), default="research",
                          help="Original fixed-capacity benchmark or newer studies")
    generate.add_argument("--pattern", required=True, help="Pattern name; see the patterns command")
    generate.add_argument("--seed", type=int, default=1, help="Deterministic random seed")
    generate.add_argument("--requests", type=int, default=20000, help="Number of requests")
    generate.add_argument("--workload-scale", type=int, help="Research scale (290 if omitted); not cache capacity")
    generate.add_argument("--capacities", type=int, nargs="+", default=[64], help="Cache sizes, first level to last")
    generate.add_argument(
        "--strategies", nargs="+", choices=("LRU", "LFU", "ARC", "2Q", "LIRS", "REF"),
        default=["LRU"], type=str.upper, help="One policy per capacity, first level to last",
    )
    generate.add_argument("--output", type=Path, help="Directory for the generated case")
    generate.add_argument("--force", action="store_true", help="Overwrite case files")

    run = command("run", "Run an existing case without regenerating it")
    run.add_argument("case", type=Path)
    run.add_argument("--runner", type=Path, help="Executable; auto-detected from CMake builds")
    run.add_argument("--json", action="store_true", help="Print machine-readable JSON")

    compare = command("compare", "Compare policies on the same saved trace")
    compare.add_argument("case", type=Path)
    compare.add_argument("--runner", type=Path, help="Executable; auto-detected from CMake builds")
    compare.add_argument("--capacity", type=int, default=64, help="Same single-cache capacity for every policy")
    compare.add_argument("--strategies", nargs="+", choices=tuple(LEGACY_STRATEGIES),
                         default=list(LEGACY_STRATEGIES), type=str.upper, help="Policies to compare")
    compare.add_argument("--json", action="store_true")

    doctor = command("doctor", "Check prerequisites without installing anything")
    doctor.add_argument("--runner", type=Path)

    research = command("research", "Small configurable research sweep")
    research.add_argument("--mode", choices=("software", "hierarchy"), default="software",
                          help="Single-cache policies or the study II hierarchy model")
    research.add_argument("--patterns", nargs="+", choices=SOFTWARE_PATTERNS, default=["hot_cold"],
                          help="Patterns to measure; popular/navigation require software mode")
    research.add_argument("--seeds", type=int, nargs="+", default=[1], help="Distinct random seeds")
    research.add_argument("--requests", type=int, default=20000, help="Requests per pattern and seed")
    research.add_argument("--workload-scale", type=int, default=290, help="Workload size, not cache capacity")
    research.add_argument("--sizes", type=int, nargs="+", default=[64], help="Software mode: single-cache sizes")
    research.add_argument("--total", type=int, default=290, help="Hierarchy mode: total cache capacity")
    research.add_argument("--runner", type=Path, help="Executable; auto-detected from CMake builds")
    research.add_argument("--output", type=Path, default=ROOT / "local_data" / "research",
                          help="Results root; a mode subdirectory will be created")
    research.add_argument("--plots", action="store_true", help="Requires matplotlib")
    research.add_argument("--plan", action="store_true", help="Save the plan without running C++")
    research.add_argument("--force", action="store_true", help="Reuse an existing results directory")

    search = command("search", "Exact search (study III); start with small total capacities")
    search.add_argument("--patterns", nargs="+", choices=SOFTWARE_PATTERNS, default=["hot_cold"],
                        help="Patterns to measure")
    search.add_argument("--totals", nargs="+", type=int, default=[8], help="Total capacities; cost grows rapidly")
    search.add_argument("--max-levels", type=int, choices=(1, 2, 3), default=3, help="Maximum hierarchy depth")
    search.add_argument("--requests", type=int, default=1000, help="Requests per pattern and seed")
    search.add_argument("--seeds", nargs="+", type=int, default=[1], help="Distinct random seeds")
    search.add_argument("--workload-scale", type=int, default=290, help="Workload size, not cache capacity")
    search.add_argument("--workers", type=int, default=1, help="Parallel pattern searches")
    search.add_argument("--runner", type=Path, help="Exact-search executable; auto-detected")
    search.add_argument("--output", type=Path, default=ROOT / "local_data" / "search", help="Results directory")
    search.add_argument("--plots", action="store_true", help="Requires matplotlib")
    search.add_argument("--force", action="store_true", help="Reuse an existing results directory")
    return parser


LEGACY_STRATEGIES = ("LRU", "LFU", "ARC", "2Q", "LIRS", "REF")


def find_runner(path=None, exact=False):
    if path is not None:
        resolved = path.resolve()
        if resolved.is_file():
            return resolved
        raise ValueError(f"Runner not found: {resolved}")
    system = platform.system()
    target = "cache_exact_search" if exact else "cache_benchmark_runner"
    executable = target + (".exe" if system == "Windows" else "")
    for preset in ("release", "debug", "release-make", "debug-make"):
        for suffix in (Path(executable), Path(preset.title()) / executable):
            candidate = PROJECT_ROOT / "build" / system / preset / suffix
            if candidate.is_file():
                return candidate.resolve()
    candidate = PROJECT_ROOT / "build-benchmark" / executable
    if candidate.is_file():
        return candidate.resolve()
    raise ValueError(
        f"No {target} found. Run: cmake --preset release; "
        f"cmake --build --preset release --target {target} --parallel. "
        "For a custom build, provide --runner PATH."
    )


def generate_case(args):
    available = REQUEST_PATTERNS if args.suite == "legacy" else SOFTWARE_PATTERNS
    if args.pattern not in available:
        raise ValueError(f"Unknown {args.suite} pattern {args.pattern!r}; choose: {', '.join(available)}")
    if args.requests < 1:
        raise ValueError("--requests must be positive")
    if args.suite == "legacy" and args.workload_scale is not None:
        raise ValueError("--workload-scale applies only to research; legacy parameters are fixed")
    scale = args.workload_scale if args.workload_scale is not None else 290
    if scale < 2 or args.requests + 12 * scale > 2147483647:
        raise ValueError("Scale must be >= 2 and generated page IDs must fit in a C++ int")
    if not 1 <= len(args.capacities) <= 3 or len(args.capacities) != len(args.strategies):
        raise ValueError("Provide 1..3 capacities and one strategy for each capacity")
    for strategy, capacity in zip(args.strategies, args.capacities):
        if not 1 <= capacity <= 2147483647:
            raise ValueError("Capacities must be in 1..2147483647")
        if strategy in ("2Q", "LIRS") and capacity < 2:
            raise ValueError(f"{strategy} needs at least two cache slots")
    if "REF" in args.strategies and len(args.strategies) != 1:
        raise ValueError("REF is supported only as a single cache")

    folder = args.output or ROOT / "local_data" / f"{args.suite}_{args.pattern}_seed{args.seed}"
    folder = folder.resolve()
    if not args.force and any((folder / name).exists() for name in FILES):
        raise ValueError(f"Case files already exist in {folder}; choose another --output or use --force")
    if args.suite == "legacy":
        requests = generate_request_sequence(args.pattern, args.seed, args.requests)
        generator_path = ROOT / "cache_benchmark.py"
    else:
        requests = generate_requests(args.pattern, args.seed, args.requests, scale)
        generator_path = ROOT / "research" / "workloads.py"
    trace = " ".join(map(str, requests))
    contents = {
        "requests.txt": (trace + "\n").encode("utf-8"),
        "input.txt": (
            " ".join(map(str, args.capacities)) + f"\n{len(requests)}\n{trace}\n"
        ).encode("utf-8"),
        "config.txt": (
            str(len(args.strategies)) + "\n" + "\n".join(args.strategies) + "\n"
        ).encode("utf-8"),
    }
    metadata = {
        "schema": 1,
        "suite": args.suite,
        "pattern": args.pattern,
        "seed": args.seed,
        "requests": len(requests),
        "workload_scale": scale if args.suite == "research" else None,
        "capacities": args.capacities,
        "strategies": args.strategies,
        "cold_start": True,
        "python": platform.python_version(),
        "generator_sha256": sha256(generator_path.read_bytes()),
        # Same canonical request hash as cache_research.py, without a trailing LF.
        "trace_sha256": sha256(trace.encode("utf-8")),
        "file_sha256": {name: sha256(content) for name, content in contents.items()},
    }
    folder.mkdir(parents=True, exist_ok=True)
    for name, content in contents.items():
        (folder / name).write_bytes(content)
    (folder / "manifest.json").write_bytes(
        (json.dumps(metadata, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    )
    return folder


def read_case(folder):
    folder = folder.resolve()
    metadata = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    if not isinstance(metadata, dict) or metadata.get("schema") != 1:
        raise ValueError("Unsupported case manifest schema")
    if not isinstance(metadata.get("file_sha256"), dict):
        raise ValueError("Case manifest is missing file checksums")
    if not isinstance(metadata.get("requests"), int) or metadata["requests"] < 1:
        raise ValueError("Case manifest has an invalid request count")
    if not isinstance(metadata.get("capacities"), list) or not 1 <= len(metadata["capacities"]) <= 3:
        raise ValueError("Case manifest has an invalid level count")
    for name in FILES[:-1]:
        if sha256((folder / name).read_bytes()) != metadata["file_sha256"].get(name):
            raise ValueError(f"{name} was modified since generation; regenerate the case")
    requests = [int(value) for value in (folder / "requests.txt").read_text().split()]
    if not requests or len(requests) != metadata["requests"]:
        raise ValueError("Invalid request count in case manifest")
    return metadata, requests


def run_case(folder, runner=None):
    folder, runner = folder.resolve(), find_runner(runner)
    metadata, _ = read_case(folder)
    process = subprocess.run(
        [str(runner), str(folder / "config.txt"), str(folder / "input.txt")],
        capture_output=True, text=True, check=False,
    )
    if process.returncode:
        raise RuntimeError(process.stderr.strip() or process.stdout.strip() or "Runner failed")
    counters = [int(value) for value in process.stdout.split()]
    check_statistics(counters, metadata["requests"], len(metadata["capacities"]))
    return {
        "requests": counters[0], "hits": counters[1], "misses": counters[2],
        "hit_rate": counters[1] / counters[0],
        "levels": [
            dict(zip(("requests", "hits", "misses"), counters[3 + 3 * i:6 + 3 * i]))
            for i in range(len(metadata["capacities"]))
        ],
    }


def compare_case(folder, runner, capacity, strategies):
    if not 1 <= capacity <= 2147483647:
        raise ValueError("--capacity must be in 1..2147483647")
    if capacity < 2 and any(strategy in ("2Q", "LIRS") for strategy in strategies):
        raise ValueError("2Q/LIRS need capacity >= 2; select other --strategies or increase capacity")
    from research.runner import Runner

    _, requests = read_case(folder)
    runner_path = find_runner(runner)
    rows = []
    with tempfile.TemporaryDirectory(prefix="cache-comparison-") as work:
        with Runner(runner_path, Path(work)) as process:
            for strategy in dict.fromkeys(strategies):
                counters = process.run((strategy,), (capacity,), requests)
                rows.append({"strategy": strategy, "capacity": capacity,
                             "requests": counters[0], "hits": counters[1],
                             "misses": counters[2], "hit_rate": counters[1] / counters[0]})
    return rows


def research_sweep(args):
    import cache_research

    target = args.output.resolve() / args.mode
    if target.exists() and any(target.iterdir()) and not args.force:
        raise ValueError(f"Results already exist in {target}; choose --output or use --force")
    options = ["--mode", args.mode, "--root", str(args.output),
               "--requests", str(args.requests), "--workload-scale", str(args.workload_scale),
               "--total", str(args.total), "--patterns", *args.patterns,
               "--seeds", *map(str, args.seeds), "--sizes", *map(str, args.sizes)]
    if args.plan:
        options.append("--generate-only")
    else:
        options.extend(["--runner", str(find_runner(args.runner))])
    if not args.plots:
        options.append("--no-plots")
    parsed = cache_research.read_arguments(options)
    layouts, patterns = cache_research.experiment_plan(args.mode, parsed)
    print(f"{len(patterns)} pattern(s), {len(args.seeds)} seed(s), {len(layouts)} layout(s).")
    cache_research.run_experiment(args.mode, parsed)
    print(f"Results directory: {target}")


def exact_search(args):
    import cache_unrestricted

    folder = args.output.resolve()
    if folder.exists() and any(folder.iterdir()) and not args.force:
        raise ValueError(f"Results already exist in {folder}; choose --output or use --force")
    if args.plots:
        try:
            import matplotlib
        except ImportError as error:
            raise ValueError("Plots require matplotlib; omit --plots or install it") from error
    options = ["--runner", str(find_runner(args.runner, exact=True)), "--output", str(folder),
               "--patterns", *args.patterns, "--totals", *map(str, args.totals),
               "--requests", str(args.requests), "--seeds", *map(str, args.seeds),
               "--workload-scale", str(args.workload_scale), "--workers", str(args.workers),
               "--max-levels", str(args.max_levels)]
    if not args.plots:
        options.append("--no-plots")
    print("Exact search can grow rapidly with total capacity; selected totals:", args.totals)
    cache_unrestricted.main(options)


def main(argv=None):
    parser = make_parser()
    args = parser.parse_args(argv)
    try:
        if args.command == "patterns":
            if args.workload_scale < 2:
                raise ValueError("--workload-scale must be >= 2")
            patterns = REQUEST_PATTERNS if args.suite == "legacy" else SOFTWARE_PATTERNS
            for pattern in patterns:
                label = (LEGACY_LABELS[pattern] if args.suite == "legacy"
                         else pattern_label(pattern, args.workload_scale))
                print(f"{pattern:16} {label}")
        elif args.command == "generate":
            folder = generate_case(args)
            print(f"Generated case: {folder}\nRequests: {folder / 'requests.txt'}")
            next_command = [sys.executable, str(ROOT / "benchmark.py"), "run", str(folder)]
            formatted = (subprocess.list2cmdline(next_command) if platform.system() == "Windows"
                         else shlex.join(next_command))
            print(f"Next: {formatted}")
        elif args.command == "doctor":
            print(f"Python: {platform.python_version()}\nGeneration: available (standard library only)")
            print(f"Runner: {find_runner(args.runner)}")
        elif args.command == "research":
            research_sweep(args)
        elif args.command == "search":
            exact_search(args)
        elif args.command == "compare":
            rows = compare_case(args.case, args.runner, args.capacity, args.strategies)
            if args.json:
                print(json.dumps(rows, indent=2))
            else:
                print(f"{'Strategy':<10} {'Capacity':>8} {'Hits':>10} {'Misses':>10} {'Hit rate':>10}")
                for row in rows:
                    print(f"{row['strategy']:<10} {row['capacity']:>8} {row['hits']:>10} "
                          f"{row['misses']:>10} {row['hit_rate']:>9.2%}")
        else:
            result = run_case(args.case, args.runner)
            if args.json:
                print(json.dumps(result, ensure_ascii=False, indent=2))
            else:
                print(f"Requests: {result['requests']}\nHits: {result['hits']}\n"
                      f"Misses: {result['misses']}\nHit rate: {result['hit_rate']:.2%}")
                for index, level in enumerate(result['levels'], 1):
                    print(f"L{index}: {level['requests']} requests, {level['hits']} hits, "
                          f"{level['misses']} misses")
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        parser.exit(1, f"Benchmark failed: {error}\n")


if __name__ == "__main__":
    main()
