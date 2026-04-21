from __future__ import annotations

import csv
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent

DATASET_FILES = {
    "Generated": [
        SCRIPT_DIR / "grid_output.csv",
    ],
    "Small": [
        SCRIPT_DIR / "output_data_track1" / "track1_results.csv",
        SCRIPT_DIR / "track1_results_low_timelimit.csv",
    ],
    "Medium": [
        SCRIPT_DIR / "output_data_track2" / "track2_results1.csv",
        SCRIPT_DIR / "output_data" / "track2_results.csv",
    ],
    "Large": [
        SCRIPT_DIR / "output_data_track3" / "track3_results.csv",
    ],
}

ALGORITHMS = [
    ("Heuristic", "heuristic"),
    ("Parallel Heuristic", "parallel_heuristic"),
    ("Approximation", "approximation"),
    ("Exact", "exact"),
]


def resolve_csv(dataset_name: str) -> Path:
    for candidate in DATASET_FILES[dataset_name]:
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"Could not find CSV for dataset '{dataset_name}'.")


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def parse_bool(value: str) -> bool:
    return value.strip() in {"1", "true", "True"}


def parse_float(value: str, default: float = -1.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def was_attempted(row: dict[str, str], prefix: str) -> bool:
    execution_time = parse_float(row.get(f"{prefix}_execution_time_ms", "-1"))
    cut_size = parse_float(row.get(f"{prefix}_cut_size", "-1"))
    valid = parse_bool(row.get(f"{prefix}_valid", "0"))
    timed_out = parse_bool(row.get(f"{prefix}_timed_out", "0"))

    if prefix == "exact":
        exact_best_time = parse_float(row.get("exact_best_time", "-1"))
        return valid or timed_out or cut_size >= 0 or execution_time >= 0 or exact_best_time >= 0

    return valid or timed_out or cut_size >= 0 or execution_time >= 0


def pct(count: int, total: int) -> str:
    if total == 0:
        return "n/a"
    return f"{(100.0 * count / total):.1f}%"


def summarize_dataset(dataset_name: str) -> list[dict[str, str | int]]:
    rows = read_rows(resolve_csv(dataset_name))
    dataset_summary: list[dict[str, str | int]] = []

    for algorithm_label, prefix in ALGORITHMS:
        attempted = 0
        valid = 0
        timed_out = 0
        edge_between_inferred = 0
        other_failures = 0

        for row in rows:
            if not was_attempted(row, prefix):
                continue

            attempted += 1
            is_valid = parse_bool(row.get(f"{prefix}_valid", "0"))
            is_timed_out = parse_bool(row.get(f"{prefix}_timed_out", "0"))
            cut_size = parse_float(row.get(f"{prefix}_cut_size", "-1"))

            if is_valid:
                valid += 1
            elif is_timed_out:
                timed_out += 1
            elif cut_size < 0:
                edge_between_inferred += 1
            else:
                other_failures += 1

        dataset_summary.append(
            {
                "dataset": dataset_name,
                "algorithm": algorithm_label,
                "instances": len(rows),
                "attempted": attempted,
                "valid_count": valid,
                "valid_pct": pct(valid, attempted),
                "timeout_count": timed_out,
                "timeout_pct": pct(timed_out, attempted),
                "edge_between_inferred_count": edge_between_inferred,
                "edge_between_inferred_pct": pct(edge_between_inferred, attempted),
                "other_failure_count": other_failures,
                "other_failure_pct": pct(other_failures, attempted),
            }
        )

    return dataset_summary


def to_markdown(rows: list[dict[str, str | int]]) -> str:
    headers = [
        "Dataset",
        "Algorithm",
        "Instances",
        "Attempted",
        "Valid",
        "Valid %",
        "Timeouts",
        "Timeout %",
        "Edge Between*",
        "Edge Between* %",
        "Other Failures",
        "Other Failures %",
    ]

    lines = [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join(["---"] * len(headers)) + " |",
    ]

    for row in rows:
        lines.append(
            "| "
            + " | ".join(
                [
                    str(row["dataset"]),
                    str(row["algorithm"]),
                    str(row["instances"]),
                    str(row["attempted"]),
                    str(row["valid_count"]),
                    str(row["valid_pct"]),
                    str(row["timeout_count"]),
                    str(row["timeout_pct"]),
                    str(row["edge_between_inferred_count"]),
                    str(row["edge_between_inferred_pct"]),
                    str(row["other_failure_count"]),
                    str(row["other_failure_pct"]),
                ]
            )
            + " |"
        )

    lines.append("")
    lines.append(
        "* `Edge Between` is inferred from CSV rows where the algorithm was attempted, did not time out, "
        "did not produce a valid cut, and has no reported cut size."
    )
    return "\n".join(lines)


def write_csv(rows: list[dict[str, str | int]], output_path: Path) -> None:
    fieldnames = [
        "dataset",
        "algorithm",
        "instances",
        "attempted",
        "valid_count",
        "valid_pct",
        "timeout_count",
        "timeout_pct",
        "edge_between_inferred_count",
        "edge_between_inferred_pct",
        "other_failure_count",
        "other_failure_pct",
    ]

    with output_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    all_rows: list[dict[str, str | int]] = []
    for dataset_name in DATASET_FILES:
        all_rows.extend(summarize_dataset(dataset_name))

    csv_path = SCRIPT_DIR / "run_outcome_summary.csv"
    md_path = SCRIPT_DIR / "run_outcome_summary.md"

    write_csv(all_rows, csv_path)
    md_path.write_text(to_markdown(all_rows), encoding="utf-8")

    print(f"Saved CSV summary to: {csv_path}")
    print(f"Saved Markdown summary to: {md_path}")


if __name__ == "__main__":
    main()
