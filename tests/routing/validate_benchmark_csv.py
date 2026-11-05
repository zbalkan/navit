#!/usr/bin/env python3
"""Validate completeness and correctness of a successful benchmark CSV."""

import csv
import sys
from collections import Counter
from pathlib import Path

SIZES = {50, 200, 600, 1200, 2400}
SCENARIOS = {"chain", "skip"}
IMPLEMENTATIONS = {"legacy", "movement"}
OPERATIONS = {"full", "increase", "decrease"}
REPETITIONS = 25
FIELDS = [
    "scenario", "vertices", "edges", "repeat", "implementation",
    "operation", "elapsed_ns", "cost", "oracle_cost", "correctness",
]


def independently_expected_cost(vertices: int, scenario: str, operation: str) -> int:
    """Recalculate the declared forward-only restricted-road DAG independently."""
    penalty = 9000
    changed = 30 if operation == "increase" else 10
    distance = [[0, 0] for _ in range(vertices)]
    for point in range(vertices - 2, -1, -1):
        for arrival in (0, 1):
            restricted = point % 3 == 0
            length = changed if point == vertices // 2 else 10
            best = length + distance[point + 1][int(restricted)]
            if arrival and not restricted:
                best += penalty
            if scenario == "skip" and point + 2 < vertices:
                restricted_shortcut = point % 4 == 0
                alternative = 21 + distance[point + 2][int(restricted_shortcut)]
                if arrival and not restricted_shortcut:
                    alternative += penalty
                best = min(best, alternative)
            distance[point][arrival] = best
    return distance[0][0]


def validate(path: Path) -> None:
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != FIELDS:
            raise ValueError(f"Unexpected CSV columns: {reader.fieldnames}")
        seen = Counter()
        for row in reader:
            if None in row or any(value is None for value in row.values()):
                raise ValueError(f"Malformed benchmark row: {row}")
            scenario = row["scenario"]
            vertices = int(row["vertices"])
            repetition = int(row["repeat"])
            implementation = row["implementation"]
            operation = row["operation"]
            cost = int(row["cost"])
            expected = int(row["oracle_cost"])
            elapsed = int(row["elapsed_ns"])
            edges = int(row["edges"])
            if (
                scenario not in SCENARIOS
                or vertices not in SIZES
                or not 0 <= repetition < REPETITIONS
                or implementation not in IMPLEMENTATIONS
                or operation not in OPERATIONS
                or edges != vertices - 1 + (vertices - 2 if scenario == "skip" else 0)
                or elapsed < 0
                or expected != independently_expected_cost(vertices, scenario, operation)
                or cost != expected
                or row["correctness"] != "pass"
            ):
                raise ValueError(f"Invalid benchmark row: {row}")
            seen[scenario, vertices, repetition, implementation, operation] += 1
    expected_keys = {
        (scenario, size, repetition, implementation, operation)
        for scenario in SCENARIOS
        for size in SIZES
        for repetition in range(REPETITIONS)
        for implementation in IMPLEMENTATIONS
        for operation in OPERATIONS
    }
    if set(seen) != expected_keys or any(count != 1 for count in seen.values()):
        raise ValueError(
            f"Incomplete or duplicated benchmark: {sum(seen.values())} rows, "
            f"{len(expected_keys)} unique combinations required"
        )
    print(f"Verified {sum(seen.values())} correctness-gated benchmark rows")


if __name__ == "__main__":
    try:
        validate(Path(sys.argv[1]))
    except (ValueError, TypeError, OSError, IndexError) as exc:
        print(f"Benchmark CSV validation failed: {exc}", file=sys.stderr)
        sys.exit(1)
