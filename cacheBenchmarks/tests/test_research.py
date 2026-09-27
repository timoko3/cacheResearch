"""Model checks and optional integration with the existing C++ cache implementations."""

import itertools
import os
import tempfile
import unittest
from functools import lru_cache
from pathlib import Path

from cache_benchmark import create_cache_configurations
from cache_research import experiment_plan, read_arguments
from research.model import (
    average_access_cost,
    best_rows,
    hierarchy_layouts,
    percent_of_reference,
    software_sizes,
    valid_strategies,
)
from research.reference import hierarchy_lower_bound, min_misses
from research.results import paired_comparison, summarize
from research.runner import Runner
from research.workloads import generate_requests, read_requests


def exhaustive_misses(requests, capacity):
    @lru_cache(None)
    def solve(index, resident):
        if index == len(requests):
            return 0
        key = requests[index]
        if key in resident:
            return solve(index + 1, resident)
        if len(resident) < capacity:
            return 1 + solve(index + 1, tuple(sorted((*resident, key))))
        return 1 + min(
            solve(index + 1, tuple(sorted((set(resident) - {victim}) | {key})))
            for victim in resident
        )

    return solve(0, ())


class ModelTests(unittest.TestCase):
    def test_min_matches_exhaustive_optimum(self):
        for length in range(1, 7):
            for requests in itertools.product(range(3), repeat=length):
                for capacity in (1, 2, 3):
                    self.assertEqual(
                        min_misses(requests, capacity),
                        exhaustive_misses(requests, capacity),
                    )

    def test_capacity_constraints_replace_penalty(self):
        layouts = hierarchy_layouts(290, (1, 2, 4, 290), (16, 32, 48, 64), (4, 64, 256))
        self.assertIn((2, 32, 256), layouts)
        self.assertIn((4, 64, 222), layouts)
        self.assertNotIn((1, 16, 273), layouts)
        for sizes in layouts:
            self.assertEqual(sum(sizes), 290)
            self.assertTrue(all(size <= limit for size, limit in zip(sizes, (4, 64, 256))))
        with self.assertRaises(ValueError):
            hierarchy_layouts(290, (290,), (1,), (4, 64, 256))

    def test_cost_and_reference_percentage(self):
        counters = [4, 3, 1, 4, 1, 3, 3, 1, 2, 2, 1, 1]
        self.assertEqual(average_access_cost(counters), (4 + 9 + 20 + 50) / 4)
        self.assertEqual(percent_of_reference(2, 2), 100)
        self.assertEqual(percent_of_reference(3, 2), 150)
        with self.assertRaises(ValueError):
            percent_of_reference(1, 0)

    def test_dense_capacity_grid_and_unsupported_policies(self):
        sizes = software_sizes(290)
        self.assertTrue({35, 36, 37, 144, 145, 146, 289, 290, 291}.issubset(sizes))
        self.assertFalse(valid_strategies(("2Q",), (1,)))
        self.assertFalse(valid_strategies(("LIRS",), (1,)))
        self.assertTrue(valid_strategies(("LRU",), (1,)))

    def test_workloads_are_reproducible(self):
        first = generate_requests("uniform", 12, 100, 290)
        self.assertEqual(first, generate_requests("uniform", 12, 100, 290))
        self.assertNotEqual(first, generate_requests("uniform", 13, 100, 290))
        self.assertEqual(len(set(generate_requests("boundary_cycle", 1, 1000, 290))), 291)
        self.assertEqual(len(set(generate_requests("navigation", 1, 10000, 290))), 420)

    def test_external_ids_preserve_equality(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "requests.txt"
            path.write_text("logo.css page-a logo.css\npage-b", encoding="utf-8")
            self.assertEqual(read_requests(path), [0, 1, 0, 2])
            path.write_text("", encoding="utf-8")
            with self.assertRaises(ValueError):
                read_requests(path)

    def test_comparison_pairs_the_same_seeds(self):
        rows = [
            {
                "pattern": "uniform",
                "seed": seed,
                "capacities": "2",
                "strategies": strategy,
                "misses_per_1000": misses,
            }
            for seed, strategy, misses in (
                (1, "LRU", 200),
                (2, "LRU", 900),
                (1, "ARC", 190),
                (2, "ARC", 905),
            )
        ]
        arc = next(row for row in paired_comparison(rows) if row["strategy"] == "ARC")
        self.assertEqual(arc["mean_extra_misses_per_1000_vs_lru"], -2.5)
        self.assertEqual(arc["min_extra_misses_per_1000_vs_lru"], -10)
        self.assertEqual(arc["max_extra_misses_per_1000_vs_lru"], 5)
        self.assertEqual(arc["runs_better_than_lru"], 1)

    def test_ties_are_preserved(self):
        rows = [{"misses_per_1000": value} for value in (1, 1, 2)]
        self.assertEqual(len(best_rows(rows, "misses_per_1000")), 2)

    def test_original_benchmark_and_new_modes_remain_separate(self):
        configurations = create_cache_configurations()
        self.assertEqual(len(configurations), 205)
        self.assertTrue(all(sum(capacities) == 64 for _, _, capacities in configurations))
        args = read_arguments(["--generate-only", "--mode", "software", "--sizes", "2", "8"])
        layouts, patterns = experiment_plan("software", args)
        self.assertEqual(layouts, [(2,), (8,)])
        self.assertIn("navigation", patterns)

    @unittest.skipUnless(
        os.environ.get("CACHE_RESEARCH_RUNNER"),
        "Set CACHE_RESEARCH_RUNNER for C++ integration",
    )
    def test_batch_reference_and_bound(self):
        with tempfile.TemporaryDirectory() as folder:
            with Runner(Path(os.environ["CACHE_RESEARCH_RUNNER"]), Path(folder)) as runner:
                for pattern in ("boundary_cycle", "uniform", "hot_cold"):
                    requests = generate_requests(pattern, 7, 80, 8)
                    for capacity in (1, 2, 5, 8):
                        counters = runner.run(("REF",), (capacity,), requests)
                        self.assertEqual(counters[2], min_misses(requests, capacity))
                    capacities = (2, 2, 4)
                    misses = {size: min_misses(requests, size) for size in (2, 4, 8)}
                    bound = hierarchy_lower_bound(capacities, (1, 3, 10), 50, 80, misses)
                    policies = ("LRU", "LFU", "ARC", "2Q", "LIRS")
                    for strategies in itertools.product(policies, repeat=3):
                        first = runner.run(strategies, capacities, requests)
                        self.assertEqual(first, runner.run(strategies, capacities, requests))
                        self.assertGreaterEqual(average_access_cost(first) + 1e-9, bound)


if __name__ == "__main__":
    unittest.main()
