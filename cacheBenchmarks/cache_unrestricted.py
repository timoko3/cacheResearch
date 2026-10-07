"""Exact capacity/policy search without access costs or per-level limits."""

import argparse
import hashlib
import json
import subprocess
import time
from concurrent.futures import ThreadPoolExecutor
from functools import partial
from pathlib import Path

from research.workloads import SOFTWARE_PATTERNS, generate_requests
from research.results import configuration_label, write_csv, write_unrestricted_results


def settle_level_tie(record, runner_path, input_path):
    """Prefer fewer levels without making the main three-level search enumerate ties."""
    best_configuration, single_configuration = record["best"], record["single"]
    if best_configuration["misses"] == single_configuration["misses"]:
        record["best"] = single_configuration
    elif len(best_configuration["capacities"]) == 3:
        output = subprocess.check_output(
            [str(runner_path), str(input_path), "2", str(record["total"])],
            text=True,
            encoding="utf-8",
        )
        two_level_result = json.loads(output)
        if two_level_result["best"]["misses"] < best_configuration["misses"]:
            raise ValueError("Two-level search contradicts the complete search")
        record["two_level_tie_check"] = {
            "runner_sha256": hashlib.sha256(Path(runner_path).read_bytes()).hexdigest(),
            "misses": two_level_result["best"]["misses"],
            "evaluated": two_level_result["evaluated"],
            "pruned": two_level_result["pruned"],
        }
        if two_level_result["best"]["misses"] == best_configuration["misses"]:
            record["best"] = two_level_result["best"]


def validate_saved(folder, runner_path, seeds):
    """Score already selected configurations on new requests without retuning them."""
    from research.runner import Runner

    metadata = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    if set(seeds) & set(metadata["seeds"]):
        raise ValueError("Validation seeds must differ from search seeds")
    if metadata.get("status") == "running":
        raise ValueError("The saved experiment is still running")

    results = json.loads((folder / "results.json").read_text(encoding="utf-8"))

    rows = []
    with Runner(runner_path.resolve(), folder / "work") as runner:
        for pattern in metadata["patterns"]:
            selected = [item for item in results if item["pattern"] == pattern]
            training_requests = [
                generate_requests(pattern, seed, metadata["requests"], metadata["workload_scale"])
                for seed in metadata["seeds"]
            ]
            for index, requests in enumerate(training_requests):
                for item in selected:
                    for name in ("single", "best"):
                        configuration = item[name]
                        actual_misses = runner.run(
                            tuple(configuration["policies"]),
                            tuple(configuration["capacities"]),
                            requests,
                        )[2]
                        if actual_misses != configuration["misses_by_run"][index]:
                            raise ValueError(
                                "Selected configuration disagrees with the original runner"
                            )

            for seed in seeds:
                requests = generate_requests(
                    pattern, seed, metadata["requests"], metadata["workload_scale"]
                )
                for item in selected:
                    misses = {}
                    for name in ("single", "best"):
                        configuration = item[name]
                        misses[name] = runner.run(
                            tuple(configuration["policies"]),
                            tuple(configuration["capacities"]),
                            requests,
                        )[2]
                    rows.append(
                        {
                            "pattern": pattern,
                            "total": item["total"],
                            "seed": seed,
                            "new_sequence": all(requests != trace for trace in training_requests),
                            "single_misses": misses["single"],
                            "best_misses": misses["best"],
                            "saved_loads": misses["single"] - misses["best"],
                        }
                    )

    write_csv(folder / "validation.csv", rows)
    metadata["validation"] = {
        "seeds": seeds,
        "configuration_selection": "unchanged_from_search",
        "training_replay_matches": True,
        "runner_sha256": hashlib.sha256(runner_path.read_bytes()).hexdigest(),
        "csv_sha256": hashlib.sha256((folder / "validation.csv").read_bytes()).hexdigest(),
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    }
    (folder / "manifest.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8"
    )


def render_saved(folder, plots=True):
    """Rebuild numeric summaries and plots without rerunning simulations."""
    metadata = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    if metadata.get("status") == "running":
        raise ValueError("The saved experiment is still running")
    results_path = folder / "results.json"
    results = json.loads(results_path.read_text(encoding="utf-8"))
    expected_points = {
        (pattern, total) for pattern in metadata["patterns"] for total in metadata["totals"]
    }
    actual_points = {(item["pattern"], item["total"]) for item in results}
    omitted_points = {
        (item["pattern"], item["total"]) for item in metadata.get("omitted_points", [])
    }
    if (
        not omitted_points.issubset(expected_points)
        or actual_points != expected_points - omitted_points
        or len(results) != len(expected_points - omitted_points)
    ):
        raise ValueError("The saved experiment has undeclared omissions or duplicate points")
    write_unrestricted_results(folder, results, metadata, plots=plots)
    metadata["results_sha256"] = hashlib.sha256(results_path.read_bytes()).hexdigest()
    metadata["analysis_source_sha256"] = {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest()
        for path in (
            Path(__file__),
            Path(__file__).parent / "research" / "results.py",
            Path(__file__).parent / "research" / "plots.py",
        )
    }
    (folder / "manifest.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8"
    )


def positive_integer(value):
    result = int(value)
    if result < 1:
        raise argparse.ArgumentTypeError("Expected a positive integer")
    return result


def run_pattern(pattern, runner_path, input_folder, max_levels, totals):
    """Search one request pattern and save each completed capacity incrementally."""
    start_time = time.monotonic()
    search_results = []
    command = [
        str(runner_path),
        str(input_folder / f"{pattern}.txt"),
        str(max_levels),
        *map(str, totals),
    ]
    with (input_folder / f"{pattern}.stderr").open("w+", encoding="utf-8") as error_log:
        with subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=error_log, text=True, encoding="utf-8"
        ) as process:
            for line in process.stdout:
                record = json.loads(line)
                record["pattern"] = pattern
                settle_level_tie(record, runner_path, input_folder / f"{pattern}.txt")
                search_results.append(record)
                (input_folder / f"{pattern}.results.json").write_text(
                    json.dumps(search_results, indent=2), encoding="utf-8"
                )
                print(
                    f'{pattern}: total={record["total"]}, '
                    f'best={configuration_label(record["best"])}, '
                    f"elapsed={time.monotonic() - start_time:.1f}s",
                    flush=True,
                )
            if process.wait() != 0:
                error_log.seek(0)
                raise RuntimeError(error_log.read())
    if [record["total"] for record in search_results] != totals:
        raise RuntimeError(f"Incomplete search for {pattern}")
    return search_results


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", type=Path)
    parser.add_argument(
        "--render-only",
        action="store_true",
        help="Rebuild CSV summaries and plots; leave Markdown reports unchanged",
    )
    parser.add_argument(
        "--validate-with", type=Path, help="Validate saved choices using the regular C++ runner"
    )
    parser.add_argument("--validation-seeds", nargs="+", type=int, default=[2, 3])
    parser.add_argument("--output", type=Path, default=Path("cacheBenchmarks/reports/unrestricted"))
    parser.add_argument(
        "--totals", nargs="+", type=positive_integer, default=[8, 16, 36, 64, 145, 290]
    )
    parser.add_argument("--max-levels", type=int, choices=(1, 2, 3), default=3)
    parser.add_argument("--requests", type=positive_integer, default=6000)
    parser.add_argument("--seeds", nargs="+", type=int, default=[1])
    parser.add_argument("--workload-scale", type=positive_integer, default=290)
    parser.add_argument(
        "--patterns", nargs="+", choices=SOFTWARE_PATTERNS, default=list(SOFTWARE_PATTERNS)
    )
    parser.add_argument("--workers", type=positive_integer, default=2)
    parser.add_argument("--no-plots", action="store_true", help="Save numeric results without matplotlib")

    args = parser.parse_args(argv)
    if args.workload_scale < 2 or args.requests + 12 * args.workload_scale > 2147483647:
        parser.error("Workload scale must be >= 2; page IDs must fit in a C++ int")
    if len(set(args.seeds)) != len(args.seeds):
        parser.error("Seeds must be distinct")
    if max(args.totals) > 2147483647:
        parser.error("Cache capacities must fit in a C++ int")
    if args.validate_with:
        validate_saved(args.output, args.validate_with, args.validation_seeds)
        render_saved(args.output, plots=not args.no_plots)
        return
    if args.render_only:
        render_saved(args.output, plots=not args.no_plots)
        return
    if args.runner is None:
        parser.error("--runner is required unless --render-only is used")

    args.runner = args.runner.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    input_folder = args.output / "inputs"
    input_folder.mkdir(exist_ok=True)

    metadata = {
        "requests": args.requests,
        "seeds": args.seeds,
        "workload_scale": args.workload_scale,
        "max_levels": args.max_levels,
        "totals": sorted(set(args.totals)),
        "patterns": list(dict.fromkeys(args.patterns)),
        "status": "running",
        "tie_break": "fewest_levels",
        "objective": "external_loads",
        "level_lookup_cost": 0,
        "capacity_limits": None,
        "input_sha256": {},
        "input_hash_format": "UTF-8 with LF line endings",
    }

    sources = [
        Path(__file__),
        Path(__file__).with_name("cache_exact_search.cpp"),
        Path(__file__).parent / "research" / "workloads.py",
        Path(__file__).parent / "research" / "results.py",
        Path(__file__).resolve().parents[1] / "cache.h",
    ]
    sources.extend(sorted((Path(__file__).resolve().parents[1] / "cache").glob("*.h")))
    metadata["source_sha256"] = {
        str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources
    }
    metadata["runner_sha256"] = hashlib.sha256(args.runner.read_bytes()).hexdigest()

    for pattern in metadata["patterns"]:
        traces = [
            generate_requests(pattern, seed, args.requests, args.workload_scale)
            for seed in args.seeds
        ]
        content = (
            str(len(traces))
            + "\n"
            + "".join(str(len(trace)) + "\n" + " ".join(map(str, trace)) + "\n" for trace in traces)
        )
        (input_folder / f"{pattern}.txt").write_bytes(content.encode("utf-8"))
        metadata["input_sha256"][pattern] = hashlib.sha256(content.encode()).hexdigest()
    (args.output / "manifest.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8"
    )

    search_pattern = partial(
        run_pattern,
        runner_path=args.runner,
        input_folder=input_folder,
        max_levels=args.max_levels,
        totals=metadata["totals"],
    )
    with ThreadPoolExecutor(max_workers=args.workers) as pool:
        results = [
            record
            for records in pool.map(search_pattern, metadata["patterns"])
            for record in records
        ]
    (args.output / "results.json").write_text(
        json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    metadata["status"] = "complete"
    (args.output / "manifest.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    render_saved(args.output, plots=not args.no_plots)
    print("Results:", args.output, flush=True)


if __name__ == "__main__":
    main()
