"""Aggregate measurements and save numeric results and figures."""

import csv
import statistics
from collections import defaultdict

from .plots import plot_configurations, plot_hierarchy, plot_software, plot_unrestricted


def write_csv(path, rows):
    if not rows:
        return

    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def summarize(raw_rows):
    rows_by_configuration = defaultdict(list)
    for row in raw_rows:
        key = (row["pattern"], row["capacities"], row["strategies"])
        rows_by_configuration[key].append(row)

    summary = []
    for (pattern, capacities, strategies), rows in sorted(rows_by_configuration.items()):
        result_row = {
            "pattern": pattern,
            "capacities": capacities,
            "strategies": strategies,
            "runs": len(rows),
        }
        for metric in (
            "misses_per_1000",
            "reference_misses_per_1000",
            "mean_access_cost",
            "percent_of_reference",
        ):
            if metric not in rows[0]:
                continue
            values = [row[metric] for row in rows]
            result_row[metric] = statistics.mean(values)
            result_row[metric + "_min"] = min(values)
            result_row[metric + "_max"] = max(values)
        summary.append(result_row)

    return summary


def paired_comparison(raw_rows):
    """Compare each policy with LRU on the same sequence, not unrelated means."""
    lru_misses_by_run = {
        (row["pattern"], row["seed"], row["capacities"]): row["misses_per_1000"]
        for row in raw_rows
        if row["strategies"] == "LRU"
    }
    rows_by_configuration = defaultdict(list)
    for row in raw_rows:
        key = (row["pattern"], row["seed"], row["capacities"])
        miss_difference = row["misses_per_1000"] - lru_misses_by_run[key]
        rows_by_configuration[(row["pattern"], row["capacities"], row["strategies"])].append(
            miss_difference
        )
    result = []
    for (pattern, capacities, strategy), differences in sorted(rows_by_configuration.items()):
        result.append(
            {
                "pattern": pattern,
                "capacity": capacities,
                "strategy": strategy,
                "mean_extra_misses_per_1000_vs_lru": statistics.mean(differences),
                "min_extra_misses_per_1000_vs_lru": min(differences),
                "max_extra_misses_per_1000_vs_lru": max(differences),
                "runs_better_than_lru": sum(value < -1e-10 for value in differences),
                "runs": len(differences),
            }
        )

    return result


def configuration_label(configuration):
    return " → ".join(
        f"{policy}({capacity})"
        for policy, capacity in zip(configuration["policies"], configuration["capacities"])
    )


def write_results(folder, raw_rows, metadata, plots=True):
    """Save measurements, aggregate tables and figures without touching Markdown."""
    folder.mkdir(parents=True, exist_ok=True)
    summary = summarize(raw_rows)
    write_csv(folder / "raw.csv", raw_rows)
    write_csv(folder / "summary.csv", summary)
    patterns, scale = metadata["patterns"], metadata["workload_scale"]
    if metadata["mode"] == "software":
        write_csv(folder / "comparison_with_lru.csv", paired_comparison(raw_rows))
        if plots:
            plot_software(folder, summary, patterns, scale)
    elif plots:
        tied_rows = plot_hierarchy(folder, summary, patterns, scale)
        write_csv(folder / "optimal_strategies_by_pattern.csv", tied_rows)

    return summary


def write_unrestricted_results(folder, results, metadata, plots=True):
    folder.mkdir(parents=True, exist_ok=True)
    requests_in_thousands = metadata["requests"] * len(metadata["seeds"]) / 1000
    rows = []

    for result_row in results:
        best_configuration = result_row["best"]
        single_configuration = result_row["single"]
        best_misses = best_configuration["misses"]
        single_misses = single_configuration["misses"]
        reference_misses = result_row["reference_misses"]
        rows.append(
            {
                "pattern": result_row["pattern"],
                "total": result_row["total"],
                "configuration": configuration_label(best_configuration),
                "levels": len(best_configuration["capacities"]),
                "misses_per_1000": best_misses / requests_in_thousands,
                "single_configuration": configuration_label(single_configuration),
                "single_misses_per_1000": single_misses / requests_in_thousands,
                "saved_loads_per_1000": (single_misses - best_misses) / requests_in_thousands,
                "percent_of_reference": 100 * best_misses / reference_misses,
                "reference_misses_per_1000": reference_misses / requests_in_thousands,
            }
        )
    write_csv(folder / "summary.csv", rows)
    if plots:
        plot_unrestricted(folder, results, metadata)
        plot_configurations(folder, results, metadata)
    return rows
