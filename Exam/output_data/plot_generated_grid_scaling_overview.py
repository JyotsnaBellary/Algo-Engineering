from __future__ import annotations

import csv
import re
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.lines import Line2D


SCRIPT_DIR = Path(__file__).resolve().parent
CSV_PATH = SCRIPT_DIR / "grid_output_server.csv"
OUTPUT_PATH = SCRIPT_DIR / "generated_grid_scaling_overview.png"

INSTANCE_PATTERN = re.compile(r"^grid_(\d+)x(\d+)_k(\d+)_rep(\d+)$")

ALGORITHMS = [
    {
        "label": "Heuristic",
        "prefix": "heuristic",
        "runtime_column": "heuristic_execution_time_ms",
        "valid_column": "heuristic_valid",
        "timeout_column": "heuristic_timed_out",
        "linestyle": "-",
        "marker": "o",
    },
    {
        "label": "Parallel Heuristic",
        "prefix": "parallel_heuristic",
        "runtime_column": "parallel_heuristic_execution_time_ms",
        "valid_column": "parallel_heuristic_valid",
        "timeout_column": "parallel_heuristic_timed_out",
        "linestyle": "--",
        "marker": "s",
    },
    {
        "label": "Approximation",
        "prefix": "approximation",
        "runtime_column": "approximation_execution_time_ms",
        "valid_column": "approximation_valid",
        "timeout_column": "approximation_timed_out",
        "linestyle": "-.",
        "marker": "^",
    },
]

GRID_COLORS = {
    (5, 5): "#4C78A8",
    (8, 8): "#F58518",
    (10, 10): "#54A24B",
    (20, 25): "#E45756",
    (25, 40): "#B279A2",
    (30, 50): "#9D755D",
    (40, 50): "#72B7B2",
}


def parse_bool(value: str) -> bool:
    return value.strip() in {"1", "true", "True"}


def parse_float(value: str) -> float | None:
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def read_rows() -> list[dict[str, str]]:
    with CSV_PATH.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def group_rows_by_grid_and_k(
    rows: list[dict[str, str]],
) -> dict[tuple[int, int], dict[int, list[dict[str, str]]]]:
    grouped: dict[tuple[int, int], dict[int, list[dict[str, str]]]] = defaultdict(
        lambda: defaultdict(list)
    )
    for row in rows:
        match = INSTANCE_PATTERN.match(row["instance_name"])
        if not match:
            continue
        grid_size = (int(match.group(1)), int(match.group(2)))
        k = int(match.group(3))
        grouped[grid_size][k].append(row)
    return grouped


def mean(values: list[float]) -> float:
    return sum(values) / len(values)


def collect_runtime_values(
    rows: list[dict[str, str]],
    runtime_column: str,
    valid_column: str,
    timeout_column: str,
) -> list[float]:
    values: list[float] = []
    for row in rows:
        if parse_bool(row.get(timeout_column, "0")):
            continue
        if not parse_bool(row.get(valid_column, "0")):
            continue
        runtime = parse_float(row.get(runtime_column, ""))
        if runtime is None or runtime < 0:
            continue
        values.append(runtime)
    return values


def collect_ratio_values(rows: list[dict[str, str]], prefix: str) -> list[float]:
    values: list[float] = []
    valid_column = f"{prefix}_valid"
    cut_column = f"{prefix}_cut_size"
    for row in rows:
        if not parse_bool(row.get(valid_column, "0")):
            continue
        if not parse_bool(row.get("exact_valid", "0")):
            continue
        cut = parse_float(row.get(cut_column, ""))
        exact_cut = parse_float(row.get("exact_cut_size", ""))
        if cut is None or exact_cut is None:
            continue
        if cut < 0 or exact_cut <= 0:
            continue
        values.append(cut / exact_cut)
    return values


def build_series():
    rows = read_rows()
    grouped = group_rows_by_grid_and_k(rows)

    grid_sizes = sorted(grouped.keys(), key=lambda size: size[0] * size[1])
    all_k_values = sorted({k for by_k in grouped.values() for k in by_k.keys()})

    runtime_summary: dict[tuple[str, tuple[int, int]], list[float]] = {}
    ratio_summary: dict[tuple[str, tuple[int, int]], list[float]] = {}

    for algorithm in ALGORITHMS:
        for grid_size in grid_sizes:
            runtime_points: list[float] = []
            ratio_points: list[float] = []

            for k in all_k_values:
                rows_for_group = grouped[grid_size].get(k, [])
                runtime_values = collect_runtime_values(
                    rows_for_group,
                    algorithm["runtime_column"],
                    algorithm["valid_column"],
                    algorithm["timeout_column"],
                )
                ratio_values = collect_ratio_values(rows_for_group, algorithm["prefix"])

                runtime_points.append(mean(runtime_values) if runtime_values else np.nan)
                ratio_points.append(mean(ratio_values) if ratio_values else np.nan)

            runtime_summary[(algorithm["prefix"], grid_size)] = runtime_points
            ratio_summary[(algorithm["prefix"], grid_size)] = ratio_points

    return grid_sizes, all_k_values, runtime_summary, ratio_summary


def plot_scaling_overview() -> None:
    grid_sizes, k_values, runtime_summary, ratio_summary = build_series()

    fig, (ax_top, ax_bottom) = plt.subplots(
        2,
        1,
        sharex=True,
        figsize=(11.2, 8.0),
        gridspec_kw={"height_ratios": [1.15, 1.0], "hspace": 0.16},
    )

    for algorithm in ALGORITHMS:
        for grid_size in grid_sizes:
            color = GRID_COLORS[grid_size]
            runtime_values = runtime_summary[(algorithm["prefix"], grid_size)]
            ratio_values = ratio_summary[(algorithm["prefix"], grid_size)]

            ax_top.plot(
                k_values,
                runtime_values,
                linestyle=algorithm["linestyle"],
                marker=algorithm["marker"],
                linewidth=1.7,
                markersize=5.5,
                color=color,
                alpha=0.95,
            )
            ax_bottom.plot(
                k_values,
                ratio_values,
                linestyle=algorithm["linestyle"],
                marker=algorithm["marker"],
                linewidth=1.7,
                markersize=5.5,
                color=color,
                alpha=0.95,
            )

    ax_top.set_yscale("log")
    ax_top.set_ylabel("Mean Runtime (ms, log scale)", fontsize=11)
    ax_top.set_title("Generated Grid Scaling on Server: Runtime and Solution Quality", fontsize=14, pad=10)
    ax_top.grid(axis="y", which="major", linestyle=(0, (5, 5)), alpha=0.35)
    ax_top.set_axisbelow(True)

    ax_bottom.axhline(1.0, color="#666666", linestyle=(0, (4, 4)), linewidth=1.0)
    ax_bottom.text(
        k_values[-1] + 1.4,
        1.0,
        "exact",
        va="center",
        ha="left",
        fontsize=9,
        color="#666666",
    )
    ax_bottom.set_ylabel("Mean Cut-Size Ratio to Exact", fontsize=11)
    ax_bottom.set_xlabel("Terminal Count $k$", fontsize=11)
    ax_bottom.grid(axis="y", linestyle=(0, (5, 5)), alpha=0.35)
    ax_bottom.set_axisbelow(True)

    ax_bottom.set_xticks(k_values)
    ax_bottom.set_xlim(min(k_values) - 1, max(k_values) + 4)

    color_handles = [
        Line2D(
            [0],
            [0],
            color=GRID_COLORS[grid_size],
            linewidth=2.6,
            label=f"{grid_size[0]}x{grid_size[1]} ({grid_size[0] * grid_size[1]} nodes)",
        )
        for grid_size in grid_sizes
    ]

    style_handles = [
        Line2D(
            [0],
            [0],
            color="#333333",
            linestyle=algorithm["linestyle"],
            marker=algorithm["marker"],
            linewidth=2.0,
            markersize=6,
            label=algorithm["label"],
        )
        for algorithm in ALGORITHMS
    ]

    legend_grids = fig.legend(
        handles=color_handles,
        loc="upper center",
        ncol=4,
        bbox_to_anchor=(0.5, 0.99),
        frameon=True,
        fancybox=False,
        edgecolor="black",
        fontsize=9,
        title="Grid Size",
        title_fontsize=10,
        columnspacing=1.3,
    )
    fig.add_artist(legend_grids)

    fig.legend(
        handles=style_handles,
        loc="upper center",
        ncol=3,
        bbox_to_anchor=(0.5, 0.915),
        frameon=True,
        fancybox=False,
        edgecolor="black",
        fontsize=9,
        title="Algorithm",
        title_fontsize=10,
        columnspacing=1.5,
    )

    note = (
        "Top: mean runtime over valid non-timed-out runs only. "
        "Bottom: mean cut-size ratio on instances where both Exact and the respective algorithm returned valid cuts."
    )
    fig.text(0.5, 0.015, note, ha="center", fontsize=10)
    fig.subplots_adjust(top=0.82, bottom=0.1)

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT_PATH, dpi=300, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    plot_scaling_overview()
    print(f"Saved plot to: {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
