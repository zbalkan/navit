#!/usr/bin/env python3
"""Summarize correctness-validated Navit routing benchmark measurements.

Timing numbers are descriptive CI measurements, not calibrated CPU benchmarks.
No speedup is calculated across different routing cost models.
"""

import argparse
import csv
import json
import math
import statistics
from collections import defaultdict
from pathlib import Path

from validate_benchmark_csv import MODES, MOVEMENT, REPETITIONS, validate


def nearest_rank(values: list[int], percentile: float) -> int:
    """Return an explicitly defined nearest-rank percentile."""
    if not values or not 0 < percentile <= 1:
        raise ValueError("A nonempty sample and percentile in (0, 1] are required")
    ordered = sorted(values)
    return ordered[math.ceil(percentile * len(ordered)) - 1]


def describe(values: list[int]) -> dict:
    if len(values) != REPETITIONS:
        raise ValueError(f"Expected {REPETITIONS} timing samples, got {len(values)}")
    return {
        "samples": len(values),
        "median_ns": statistics.median(values),
        "p95_ns": nearest_rank(values, 0.95),
        "min_ns": min(values),
        "max_ns": max(values),
    }


def summarize(csv_path: Path, mode: str) -> dict:
    """Require complete, oracle-validated CSV input before computing statistics."""
    validate(csv_path, mode)
    timings = defaultdict(dict)
    with csv_path.open(newline="", encoding="utf-8") as stream:
        for row in csv.DictReader(stream):
            key = (row["scenario"], int(row["vertices"]), row["operation"], row["implementation"])
            timings[key][int(row["repeat"])] = int(row["elapsed_ns"])

    results = []
    for scenario in ("chain", "skip"):
        for size in (50, 200, 600, 1200, 2400):
            for operation in ("full", "increase", "decrease"):
                key = (scenario, size, operation)
                reference = timings[(*key, MOVEMENT)]
                point = timings[(*key, mode)]
                ids = sorted(point)
                if ids != sorted(reference) or len(ids) != REPETITIONS:
                    raise ValueError(f"Unpaired benchmark timings: {key}")
                point_ns = [point[i] for i in ids]
                movement_ns = [reference[i] for i in ids]
                paired_ratios = [
                    reference[i] / point[i] for i in ids if point[i] > 0
                ]
                # Exclude zero-duration denominator samples rather than invent a ratio.
                results.append({
                    "scenario": scenario,
                    "vertices": size,
                    "operation": operation,
                    "point": describe(point_ns),
                    "movement": describe(movement_ns),
                    "paired_median_movement_over_point": (
                        statistics.median(paired_ratios) if len(paired_ratios) == REPETITIONS else None
                    ),
                })
    return {
        "schema": 1,
        "mode": mode,
        "through_traffic_penalty": MODES[mode],
        "point_implementation": mode,
        "reference_implementation": MOVEMENT,
        "timing_unit": "ns",
        "percentile_method": "nearest-rank",
        "comparison": "within-mode only; positive ratio means point search is faster",
        "warmups_per_case": 3,
        "samples_per_case": REPETITIONS,
        "results": results,
    }


def to_markdown(report: dict) -> str:
    mode = report["mode"]
    lines = [
        f"# Routing benchmark: {mode}",
        "",
        f"Through-traffic penalty: {report['through_traffic_penalty']}. "
        "All costs were independently validated before timing statistics.",
        "",
        "Times exclude graph creation, reset, output, and oracle evaluation. "
        "Update samples include point-search invalidation and repair. "
        "The movement-state reference performs a full search on every update.",
        "",
        "The speed ratio is the median of paired movement/point timings; "
        "above 1 means the point search was faster in that scenario. "
        "Zero-duration denominators produce an undefined ratio. "
        "Do not compare speed ratios between different benchmark modes.",
        "",
        "| Scenario | Vertices | Operation | Point median (ms) | Point p95 (ms) | Movement median (ms) | Movement p95 (ms) | Paired ratio |",
        "| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for item in report["results"]:
        point, movement = item["point"], item["movement"]
        ratio = item["paired_median_movement_over_point"]
        formatted_ratio = f"{ratio:.2f}x" if ratio is not None else "n/a"
        lines.append(
            f"| {item['scenario']} | {item['vertices']} | {item['operation']} "
            f"| {point['median_ns'] / 1e6:.3f} | {point['p95_ns'] / 1e6:.3f} "
            f"| {movement['median_ns'] / 1e6:.3f} | {movement['p95_ns'] / 1e6:.3f} "
            f"| {formatted_ratio} |"
        )
    lines.extend([
        "",
        "## Interpretation limits",
        "",
        "These are 25 samples per scenario/size/operation from a shared CI runner, "
        "not independent hardware trials. No statistical confidence interval, "
        "regression threshold, memory result, or map-backed navigation conclusion is claimed.",
        "The two modes have different through-traffic penalties and cannot be "
        "directly compared as an algorithm speedup.",
        "",
    ])
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--mode", choices=sorted(MODES), required=True)
    parser.add_argument("--json-out", type=Path, required=True)
    parser.add_argument("--markdown-out", type=Path, required=True)
    args = parser.parse_args()
    report = summarize(args.csv, args.mode)
    args.json_out.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    args.markdown_out.write_text(to_markdown(report), encoding="utf-8")
    print(f"Summarized {len(report['results'])} benchmark strata in {args.mode} mode")


if __name__ == "__main__":
    main()
