"""Regression tests for correctness-first benchmark reporting."""

import csv
import tempfile
import unittest
from pathlib import Path

from summarize_benchmark import describe, nearest_rank, summarize, to_markdown
from validate_benchmark_csv import (
    FIELDS,
    MODES,
    REPETITIONS,
    independently_expected_cost,
)


class BenchmarkSummaryTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "samples.csv"

    def create_fixture(self, mode: str, corrupt: str | None = None) -> None:
        rows = []
        for scenario in ("chain", "skip"):
            for size in (50, 200, 600, 1200, 2400):
                edges = size - 1 + (size - 2 if scenario == "skip" else 0)
                for operation in ("full", "increase", "decrease"):
                    cost = independently_expected_cost(size, scenario, operation, MODES[mode])
                    for repetition in range(REPETITIONS):
                        for implementation, factor in ((mode, 1), ("movement", 3)):
                            rows.append({
                                "scenario": scenario,
                                "vertices": size,
                                "edges": edges,
                                "repeat": repetition,
                                "implementation": implementation,
                                "operation": operation,
                                "elapsed_ns": (1000 + size + repetition) * factor,
                                "cost": cost,
                                "oracle_cost": cost,
                                "correctness": "pass",
                            })
        if corrupt == "missing":
            rows.pop()
        elif corrupt == "mismatched-oracle":
            rows[0]["cost"] = int(rows[0]["cost"]) + 1
            rows[0]["oracle_cost"] = rows[0]["cost"]
        elif corrupt == "mode":
            rows[0]["implementation"] = "legacy" if mode == "arrival-experiment" else "arrival-experiment"
        with self.path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS)
            writer.writeheader()
            writer.writerows(rows)

    def test_nearest_rank_p95(self):
        self.assertEqual(nearest_rank(list(range(1, 26)), 0.95), 24)
        with self.assertRaises(ValueError):
            nearest_rank([], 0.95)

    def test_requires_complete_stratum(self):
        with self.assertRaisesRegex(ValueError, "Expected 25"):
            describe([1, 2])

    def test_legacy_and_experiment_separately(self):
        for mode in MODES:
            with self.subTest(mode=mode):
                self.create_fixture(mode)
                report = summarize(self.path, mode)
                self.assertEqual(len(report["results"]), 30)
                self.assertEqual(report["through_traffic_penalty"], MODES[mode])
                self.assertEqual(report["results"][0]["point"]["samples"], REPETITIONS)
                self.assertEqual(report["results"][0]["paired_median_movement_over_point"], 3)
                self.assertIn(f"# Routing benchmark: {mode}", to_markdown(report))

    def test_rejects_missing_or_corrupt_data(self):
        for defect in ("missing", "mismatched-oracle", "mode"):
            with self.subTest(defect=defect):
                self.create_fixture("arrival-experiment", corrupt=defect)
                with self.assertRaises(ValueError):
                    summarize(self.path, "arrival-experiment")


if __name__ == "__main__":
    unittest.main()
