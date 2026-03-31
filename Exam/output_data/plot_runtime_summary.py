from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
from statistics import median

import matplotlib.pyplot as plt
import numpy as np


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
    ("Heuristic", "heuristic_execution_time_ms", "heuristic_timed_out", "heuristic_valid", "#F4CAE4"),
    (
        "Parallel Heuristic",
        "parallel_heuristic_execution_time_ms",
        "parallel_heuristic_timed_out",
        "parallel_heuristic_valid",
        "#B3E2CD",
    ),
    (
        "Approximation",
        "approximation_execution_time_ms",
        "approximation_timed_out",
        "approximation_valid",
        "#CBD5E8",
    ),
    ("Exact", "exact_best_time", "exact_timed_out", "exact_valid", "#FDCDAC"),
]


def resolve_csv(dataset_name: str) -> Path:
    for candidate in DATASET_FILES[dataset_name]:
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"Could not find CSV for dataset '{dataset_name}'.")


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def row_timed_out(row: dict[str, str], timeout_column: str) -> bool:
    return row.get(timeout_column, "").strip() in {"1", "true", "True"}


def row_valid(row: dict[str, str], valid_column: str) -> bool:
    return row.get(valid_column, "").strip() in {"1", "true", "True"}


def collect_positive_values(
    rows: list[dict[str, str]],
    value_column: str,
    timeout_column: str,
    valid_column: str,
) -> list[float]:
    values: list[float] = []
    for row in rows:
        if row_timed_out(row, timeout_column):
            continue
        if not row_valid(row, valid_column):
            continue
        raw = row.get(value_column, "")
        if raw == "":
            continue
        try:
            value = float(raw)
        except ValueError:
            continue
        if value >= 0:
            values.append(value)
    return values


def aggregate(values: list[float], metric: str) -> float | None:
    if not values:
        return None
    if metric == "mean":
        return sum(values) / len(values)
    if metric == "median":
        return median(values)
    if metric == "gmean":
        safe_values = [max(v, 1e-12) for v in values]
        return math.exp(sum(math.log(v) for v in safe_values) / len(safe_values))
    raise ValueError(f"Unsupported metric: {metric}")


def format_value(value: float) -> str:
    if value >= 1000:
        return f"{value:,.0f}"
    if value >= 100:
        return f"{value:.1f}"
    if value >= 1:
        return f"{value:.2f}"
    return f"{value:.3f}"


def build_summary(metric: str) -> dict[str, list[float | None]]:
    summary: dict[str, list[float | None]] = {}
    for dataset_name in DATASET_FILES:
        rows = read_rows(resolve_csv(dataset_name))
        dataset_values: list[float | None] = []
        for _, value_column, timeout_column, valid_column, _ in ALGORITHMS:
            values = collect_positive_values(rows, value_column, timeout_column, valid_column)
            dataset_values.append(aggregate(values, metric))
        summary[dataset_name] = dataset_values
    return summary


def plot_summary(metric: str, output_path: Path) -> None:
    summary = build_summary(metric)
    dataset_names = list(DATASET_FILES.keys())

    x = np.arange(len(dataset_names))
    width = 0.18

    all_positive = [
        value
        for values in summary.values()
        for value in values
        if value is not None and value > 0
    ]
    if not all_positive:
        raise RuntimeError("No runtime values found to plot.")

    min_positive = min(all_positive)
    max_positive = max(all_positive)

    fig, ax = plt.subplots(figsize=(12, 7))

    for index, (label, _, _, _, color) in enumerate(ALGORITHMS):
        offsets = x + (index - 1.5) * width
        heights = [
            summary[dataset][index] if summary[dataset][index] is not None else np.nan
            for dataset in dataset_names
        ]

        bars = ax.bar(
            offsets,
            heights,
            width=width,
            label=label,
            color=color,
            edgecolor="black",
            linewidth=0.8,
            alpha=0.9,
        )

        for dataset_index, bar in enumerate(bars):
            value = heights[dataset_index]
            center_x = bar.get_x() + bar.get_width() / 2
            if value is None or np.isnan(value):
                ax.text(
                    center_x,
                    min_positive / 1.8,
                    "n/a",
                    ha="center",
                    va="bottom",
                    rotation=90,
                    fontsize=9,
                    color="#444444",
                )
                continue

            ax.text(
                center_x,
                value * 1.12,
                format_value(value),
                ha="center",
                va="bottom",
                rotation=90,
                fontsize=9,
                color=color,
            )

    metric_label = {
        "mean": "Average",
        "median": "Median",
        "gmean": "Geometric Mean",
    }[metric]

    ax.set_yscale("log")
    ax.set_ylim(min_positive / 2.5, max_positive * 3.5)
    ax.set_xticks(x)
    ax.set_xticklabels(dataset_names, fontsize=12)
    ax.set_ylabel(f"{metric_label} Runtime (ms, log scale)", fontsize=12)
    ax.set_title("Runtime Comparison Across Datasets", fontsize=14, pad=14)
    ax.grid(axis="y", which="major", linestyle=(0, (5, 5)), alpha=0.45)
    ax.set_axisbelow(True)
    ax.legend(
        loc="upper left",
        bbox_to_anchor=(0.02, 1.02),
        ncol=4,
        frameon=True,
        fancybox=False,
        edgecolor="black",
        fontsize=10,
        handlelength=1.2,
        columnspacing=1.2,
    )

    note = (
        "Only non-timed-out runs with valid cuts are included. Exact bars use time to best cut. "
        "Missing runs are marked as n/a."
    )
    fig.text(0.5, 0.02, note, ha="center", fontsize=10)
    fig.tight_layout(rect=(0, 0.05, 1, 1))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=300, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Plot one grouped runtime chart from benchmark CSV files."
    )
    parser.add_argument(
        "--metric",
        choices=("mean", "median", "gmean"),
        default="mean",
        help="Aggregation used per dataset and algorithm.",
    )
    parser.add_argument(
        "--output",
        default=str(SCRIPT_DIR / "runtime_summary.png"),
        help="Path of the generated image.",
    )
    args = parser.parse_args()

    output_path = Path(args.output).resolve()
    plot_summary(args.metric, output_path)
    print(f"Saved plot to: {output_path}")


if __name__ == "__main__":
    main()
