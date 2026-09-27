"""Cross-check exact pruning against the existing full hierarchy simulator."""

import itertools
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

from research.model import valid_strategies
from research.reference import min_misses
from research.runner import Runner
from research.workloads import generate_requests

POLICIES = ("LRU", "LFU", "ARC", "2Q", "LIRS")


def splits(total, levels):
    if levels == 1:
        yield (total,)
    else:
        for first in range(1, total - levels + 2):
            for tail in splits(total - first, levels - 1):
                yield (first, *tail)


@unittest.skipUnless(
    os.environ.get("CACHE_EXACT_SEARCH") and os.environ.get("CACHE_RESEARCH_RUNNER"),
    "Set both C++ executable paths for exact-search integration",
)
class ExactSearchTests(unittest.TestCase):
    def test_search_matches_unpruned_hierarchies(self):
        with tempfile.TemporaryDirectory() as name:
            folder = Path(name)
            with Runner(Path(os.environ["CACHE_RESEARCH_RUNNER"]), folder / "runner") as runner:
                for pattern in ("uniform", "hot_scan", "boundary_cycle"):
                    traces = [generate_requests(pattern, seed, 45, 8) for seed in (1, 2, 1)]
                    trace_file = folder / "traces.txt"
                    trace_file.write_text(
                        str(len(traces))
                        + "\n"
                        + "".join(
                            str(len(trace)) + "\n" + " ".join(map(str, trace)) + "\n"
                            for trace in traces
                        ),
                        encoding="utf-8",
                    )
                    for levels in (1, 2, 3):
                        output = subprocess.check_output(
                            [
                                os.environ["CACHE_EXACT_SEARCH"],
                                str(trace_file),
                                str(levels),
                                "3",
                                "5",
                            ],
                            text=True,
                        )
                        for line in output.splitlines():
                            result = json.loads(line)
                            total = result["total"]
                            best = sum(len(trace) for trace in traces)
                            for depth in range(1, levels + 1):
                                for capacities in splits(total, depth):
                                    for policies in itertools.product(POLICIES, repeat=depth):
                                        if not valid_strategies(policies, capacities):
                                            continue
                                        misses = sum(
                                            runner.run(policies, capacities, trace)[2]
                                            for trace in traces
                                        )
                                        best = min(best, misses)
                            self.assertEqual(result["best"]["misses"], best)
                            self.assertEqual(
                                result["reference_misses"],
                                sum(min_misses(trace, total) for trace in traces),
                            )
                            for field in ("single", "best"):
                                chosen = result[field]
                                replay = [
                                    runner.run(
                                        tuple(chosen["policies"]),
                                        tuple(chosen["capacities"]),
                                        trace,
                                    )[2]
                                    for trace in traces
                                ]
                                self.assertEqual(chosen["misses_by_run"], replay)
                                self.assertEqual(chosen["misses"], sum(replay))

    def test_equal_minimum_prefers_fewer_levels(self):
        from cache_unrestricted import settle_level_tie

        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "traces.txt"
            requests = generate_requests("bursts", 1, 6000, 290)
            path.write_text("1\n6000\n" + " ".join(map(str, requests)), encoding="utf-8")
            runner = Path(os.environ["CACHE_EXACT_SEARCH"])
            record = json.loads(
                subprocess.check_output([str(runner), str(path), "3", "36"], text=True)
            )
            minimum = record["best"]["misses"]
            settle_level_tie(record, runner, path)
            self.assertEqual(record["best"]["misses"], minimum)
            self.assertLess(minimum, record["single"]["misses"])
            self.assertEqual(len(record["best"]["capacities"]), 2)


if __name__ == "__main__":
    unittest.main()
