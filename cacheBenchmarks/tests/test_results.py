"""Result writers must preserve manually edited Markdown reports."""

import csv
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from research.results import write_results, write_unrestricted_results


class ResultWriterTests(unittest.TestCase):
    def prepare_folder(self, root, mode):
        folder = root / mode
        folder.mkdir()
        for path in (root / (mode + ".md"), folder / "report.md"):
            path.write_bytes(b"Manually edited report\r\n")
        return folder

    def assert_reports_unchanged(self, root, before):
        after = {path.relative_to(root): path.read_bytes() for path in root.rglob("*.md")}
        self.assertEqual(after, before)

    def test_regular_outputs_preserve_reports(self):
        for mode in ("software", "hierarchy"):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as name:
                root = Path(name)
                folder = self.prepare_folder(root, mode)
                before = {path.relative_to(root): path.read_bytes() for path in root.rglob("*.md")}
                row = {
                    "pattern": "uniform",
                    "seed": 1,
                    "capacities": "4" if mode == "software" else "1+2+7",
                    "strategies": "LRU" if mode == "software" else "LRU+LRU+LRU",
                    "misses_per_1000": 500.0,
                    "percent_of_reference": 100.0,
                    "reference_misses_per_1000": 500.0,
                }
                metadata = {"mode": mode, "patterns": ["uniform"], "workload_scale": 8}
                with patch("research.results.plot_software"), patch(
                    "research.results.plot_hierarchy", return_value=[row]
                ):
                    write_results(folder, [row], metadata)
                self.assert_reports_unchanged(root, before)
                with (folder / "summary.csv").open(encoding="utf-8") as stream:
                    summary = list(csv.DictReader(stream))
                self.assertEqual(float(summary[0]["misses_per_1000"]), 500.0)

    def test_unrestricted_outputs_preserve_reports_and_metrics(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            folder = self.prepare_folder(root, "unrestricted")
            before = {path.relative_to(root): path.read_bytes() for path in root.rglob("*.md")}
            records = [
                {
                    "pattern": "uniform",
                    "total": 4,
                    "reference_misses": 4,
                    "single": {"capacities": [4], "policies": ["LRU"], "misses": 8},
                    "best": {"capacities": [1, 3], "policies": ["LRU", "LFU"], "misses": 6},
                }
            ]
            with patch("research.results.plot_unrestricted"), patch(
                "research.results.plot_configurations"
            ):
                write_unrestricted_results(folder, records, {"requests": 10, "seeds": [1]})
            self.assert_reports_unchanged(root, before)
            with (folder / "summary.csv").open(encoding="utf-8") as stream:
                row = next(csv.DictReader(stream))
            self.assertEqual(float(row["misses_per_1000"]), 600.0)
            self.assertEqual(float(row["saved_loads_per_1000"]), 200.0)
            self.assertEqual(float(row["percent_of_reference"]), 150.0)


if __name__ == "__main__":
    unittest.main()
