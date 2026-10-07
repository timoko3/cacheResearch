"""Regression tests for reproducing benchmark traces without a C++ build."""

import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BENCHMARKS = Path(__file__).resolve().parents[1] / "cacheBenchmarks"
sys.path.insert(0, str(BENCHMARKS))

import benchmark
import cache_research
from cache_benchmark import REQUEST_PATTERNS, generate_request_sequence
from research.workloads import SOFTWARE_PATTERNS, generate_requests


class BenchmarkCliTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="benchmark-cli-")
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)

    def generate(self, *options, folder=None):
        args = benchmark.make_parser().parse_args(
            ["generate", "--pattern", "hot_cold", "--requests", "100",
             "--output", str(folder or self.folder / "case"), *options]
        )
        return benchmark.generate_case(args)

    def test_every_pattern_matches_original_generator(self):
        for suite, patterns in (("legacy", REQUEST_PATTERNS), ("research", SOFTWARE_PATTERNS)):
            for pattern in patterns:
                with self.subTest(suite=suite, pattern=pattern):
                    folder = self.generate("--suite", suite, "--pattern", pattern, "--seed", "3",
                                           folder=self.folder / f"{suite}_{pattern}")
                    _, actual = benchmark.read_case(folder)
                    expected = (generate_request_sequence(pattern, 3, 100) if suite == "legacy"
                                else generate_requests(pattern, 3, 100, 290))
                    self.assertEqual(actual, expected)

    def test_reproducible_and_independent_of_cache_configuration(self):
        first = self.generate("--capacities", "16")
        second = self.generate("--capacities", "8", "16", "40", "--strategies", "LRU", "2Q", "ARC",
                               folder=self.folder / "three levels")
        self.assertEqual((first / "requests.txt").read_bytes(),
                         (second / "requests.txt").read_bytes())
        meta, requests = benchmark.read_case(second)
        self.assertEqual((second / "config.txt").read_text(), "3\nLRU\n2Q\nARC\n")
        lines = (second / "input.txt").read_text().splitlines()
        self.assertEqual(lines[0], "8 16 40")
        self.assertEqual(int(lines[1]), len(requests))
        self.assertEqual(list(map(int, lines[2].split())), requests)
        self.assertEqual(meta["trace_sha256"], hashlib.sha256(lines[2].encode()).hexdigest())

    def test_seed_changes_random_trace(self):
        first = self.generate("--seed", "1")
        second = self.generate("--seed", "2", folder=self.folder / "other")
        self.assertNotEqual((first / "requests.txt").read_bytes(),
                            (second / "requests.txt").read_bytes())

    def test_scale_matches_research(self):
        folder = self.generate("--pattern", "boundary_cycle", "--workload-scale", "8")
        _, requests = benchmark.read_case(folder)
        self.assertEqual(requests, [index % 9 for index in range(100)])

    def test_existing_files_are_not_overwritten(self):
        folder = self.generate()
        before = (folder / "requests.txt").read_bytes()
        with self.assertRaises(ValueError):
            self.generate("--seed", "2")
        self.assertEqual((folder / "requests.txt").read_bytes(), before)
        self.generate("--seed", "2", "--force")
        self.assertNotEqual((folder / "requests.txt").read_bytes(), before)

    def test_mutated_input_is_detected(self):
        folder = self.generate()
        with (folder / "input.txt").open("a") as stream:
            stream.write("999\n")
        with self.assertRaisesRegex(ValueError, "input.txt was modified"):
            benchmark.read_case(folder)

    def test_invalid_manifest_has_readable_error(self):
        folder = self.generate()
        (folder / "manifest.json").write_text("[]")
        with self.assertRaisesRegex(ValueError, "manifest schema"):
            benchmark.read_case(folder)

    def test_invalid_parameters_do_not_create_case(self):
        for options in (("--requests", "0"), ("--workload-scale", "1"),
                        ("--capacities", "1", "--strategies", "2Q"),
                        ("--capacities", "8", "16"),
                        ("--suite", "legacy", "--workload-scale", "64"),
                        ("--pattern", "missing"),
                        ("--capacities", "8", "16", "--strategies", "REF", "LRU")):
            with self.subTest(options=options), self.assertRaises(ValueError):
                self.generate(*options)
        self.assertFalse((self.folder / "case").exists())

    def test_cli_from_other_directory_and_path_with_spaces(self):
        output = self.folder / "case with spaces"
        result = subprocess.run(
            [sys.executable, str(BENCHMARKS / "benchmark.py"), "generate", "--pattern", "bursts",
             "--requests", "32", "--output", str(output)],
            cwd=self.folder, capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(benchmark.read_case(output)[1]), 32)

    def test_missing_runner_has_build_instructions(self):
        with self.assertRaisesRegex(ValueError, "Runner not found"):
            benchmark.find_runner(self.folder / "missing")

    def test_research_plan_selects_only_requested_patterns(self):
        with contextlib.redirect_stdout(io.StringIO()):
            benchmark.main(["research", "--plan", "--patterns", "bursts", "--requests", "32",
                            "--output", str(self.folder / "research")])
        metadata = json.loads((self.folder / "research/software/plan.json").read_text())
        self.assertEqual(metadata["patterns"], ["bursts"])
        self.assertEqual(metadata["seeds"], [1])
        self.assertFalse(metadata["plots"])
        self.assertIn("cache/cacheLRU.h", metadata["source_hashes"])

    def test_numeric_only_research_does_not_import_matplotlib(self):
        from unittest.mock import patch
        from research.results import write_results

        row = {"pattern": "bursts", "capacities": "64", "strategies": "LRU", "seed": 1,
               "misses_per_1000": 10.0, "reference_misses_per_1000": 10.0,
               "percent_of_reference": 100.0}
        metadata = {"mode": "software", "patterns": ["bursts"], "workload_scale": 290}
        with patch.dict(sys.modules, {"matplotlib": None}):
            write_results(self.folder, [row], metadata, plots=False)
        self.assertTrue((self.folder / "summary.csv").is_file())
        self.assertTrue((self.folder / "comparison_with_lru.csv").is_file())

    def test_hierarchy_rejects_software_only_patterns(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            cache_research.read_arguments(["--mode", "hierarchy", "--patterns", "popular",
                                           "--generate-only"])

    @unittest.skipUnless(os.environ.get("CACHE_BENCHMARK_RUNNER"), "C++ runner not supplied")
    def test_saved_case_and_comparison_with_real_runner(self):
        runner = Path(os.environ["CACHE_BENCHMARK_RUNNER"])
        folder = self.generate()
        result = benchmark.run_case(folder, runner)
        self.assertEqual(result["requests"], 100)
        self.assertEqual(result["hits"] + result["misses"], 100)
        rows = benchmark.compare_case(folder, runner, 64, benchmark.LEGACY_STRATEGIES)
        self.assertEqual(len(rows), 6)
        self.assertEqual(rows[0]["misses"], result["misses"])
        self.assertLessEqual(rows[-1]["misses"], min(row["misses"] for row in rows[:-1]))
        for row in rows:
            self.assertEqual(row["hits"] + row["misses"], 100)

    @unittest.skipUnless(os.environ.get("CACHE_BENCHMARK_RUNNER"), "C++ runner not supplied")
    def test_selected_research_with_real_runner(self):
        with contextlib.redirect_stdout(io.StringIO()):
            benchmark.main(["research", "--patterns", "bursts", "--requests", "32",
                            "--sizes", "8", "--workload-scale", "8",
                            "--runner", os.environ["CACHE_BENCHMARK_RUNNER"],
                            "--output", str(self.folder / "research")])
        folder = self.folder / "research/software"
        self.assertTrue((folder / "raw.csv").is_file())
        self.assertTrue((folder / "summary.csv").is_file())
        metadata = json.loads((folder / "manifest.json").read_text())
        self.assertEqual(metadata["patterns"], ["bursts"])
        self.assertFalse(list(folder.glob("*.png")))

    @unittest.skipUnless(os.environ.get("CACHE_BENCHMARK_RUNNER"), "C++ runner not supplied")
    def test_hierarchy_with_real_runner(self):
        with contextlib.redirect_stdout(io.StringIO()):
            benchmark.main(["research", "--mode", "hierarchy", "--patterns", "bursts",
                            "--total", "33", "--requests", "32", "--workload-scale", "8",
                            "--runner", os.environ["CACHE_BENCHMARK_RUNNER"],
                            "--output", str(self.folder / "research")])
        folder = self.folder / "research/hierarchy"
        self.assertTrue((folder / "summary.csv").is_file())
        metadata = json.loads((folder / "manifest.json").read_text())
        self.assertEqual(metadata["patterns"], ["bursts"])
        self.assertFalse(list(folder.glob("*.png")))

    @unittest.skipUnless(os.environ.get("CACHE_EXACT_SEARCH_RUNNER"), "Exact-search runner not supplied")
    def test_small_exact_search_with_real_runner(self):
        with contextlib.redirect_stdout(io.StringIO()):
            benchmark.main(["search", "--patterns", "bursts", "--requests", "32",
                            "--totals", "2", "--max-levels", "1", "--workload-scale", "8",
                            "--runner", os.environ["CACHE_EXACT_SEARCH_RUNNER"],
                            "--output", str(self.folder / "search")])
        self.assertTrue((self.folder / "search/summary.csv").is_file())
        metadata = json.loads((self.folder / "search/manifest.json").read_text())
        self.assertEqual(metadata["patterns"], ["bursts"])
        self.assertEqual(metadata["status"], "complete")


if __name__ == "__main__":
    unittest.main()
