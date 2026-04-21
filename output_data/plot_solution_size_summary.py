from __future__ import annotations

import csv
from pathlib import Path

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
    ("Heuristic", "heuristic", "#C96A72", "o"),
    ("Parallel Heuristic", "parallel_heuristic", "#B3E2CD", "s"),
    ("Approximation", "approximation", "#CBD5E8", "^"),
    ("Exact", "exact", "#FDCDAC", "D"),
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


def parse_float(value: str, default: float = float("nan")) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def mean(values: list[float]) -> float:
    return sum(values) / len(values)


def summarize_dataset_structure(rows: list[dict[str, str]]) -> tuple[float, float]:
    node_values = [
        parse_float(row.get("num_nodes", "nan"))
        for row in rows
        if not np.isnan(parse_float(row.get("num_nodes", "nan")))
    ]
    terminal_values = [
        parse_float(row.get("num_terminals", "nan"))
        for row in rows
        if not np.isnan(parse_float(row.get("num_terminals", "nan")))
    ]
    if not node_values or not terminal_values:
        raise RuntimeError("Missing num_nodes or num_terminals data in benchmark CSV.")
    return mean(node_values), mean(terminal_values)


def valid_cut_sizes(rows: list[dict[str, str]], prefix: str) -> list[float]:
    sizes: list[float] = []
    valid_column = f"{prefix}_valid"
    cut_size_column = f"{prefix}_cut_size"

    for row in rows:
        if not parse_bool(row.get(valid_column, "0")):
            continue
        cut_size = parse_float(row.get(cut_size_column, "nan"))
        if np.isnan(cut_size) or cut_size < 0:
            continue
        sizes.append(cut_size)

    return sizes


def build_summary() -> tuple[dict[str, list[float]], dict[str, tuple[float, float]], dict[str, list[int]]]:
    cut_summary: dict[str, list[float]] = {}
    structure_summary: dict[str, tuple[float, float]] = {}
    count_summary: dict[str, list[int]] = {}

    for dataset_name in DATASET_FILES:
        rows = read_rows(resolve_csv(dataset_name))
        structure_summary[dataset_name] = summarize_dataset_structure(rows)

        dataset_cuts: list[float] = []
        dataset_counts: list[int] = []

        for _, prefix, _, _ in ALGORITHMS:
            sizes = valid_cut_sizes(rows, prefix)
            dataset_counts.append(len(sizes))
            dataset_cuts.append(mean(sizes) if sizes else np.nan)

        cut_summary[dataset_name] = dataset_cuts
        count_summary[dataset_name] = dataset_counts

    return cut_summary, structure_summary, count_summary


def plot_solution_sizes(output_path: Path) -> None:
    cut_summary, structure_summary, count_summary = build_summary()
    dataset_names = list(DATASET_FILES.keys())

    x = np.arange(len(dataset_names))
    offsets = [-0.18, -0.06, 0.06, 0.18]
    fig, (ax_top, ax_bottom) = plt.subplots(
        2,
        1,
        sharex=True,
        figsize=(10.4, 7.4),
        gridspec_kw={"height_ratios": [1.15, 3.2], "hspace": 0.05},
    )

    for index, (label, _, color, marker) in enumerate(ALGORITHMS):
        values = [cut_summary[dataset][index] for dataset in dataset_names]
        counts = [count_summary[dataset][index] for dataset in dataset_names]
        x_positions = x + offsets[index]

        for ax in (ax_top, ax_bottom):
            ax.plot(
                x_positions,
                values,
                color=color,
                linewidth=2.0,
                marker=marker,
                markersize=9,
                markerfacecolor=color,
                markeredgecolor="black",
                markeredgewidth=0.9,
                label=label,
                zorder=3,
            )

    tick_labels = []
    for dataset_name in dataset_names:
        avg_nodes, avg_terminals = structure_summary[dataset_name]
        tick_labels.append(
            f"{dataset_name}\n"
            f"$\\overline{{|T|}}={avg_terminals:.1f}$\n"
            f"$\\overline{{|V|}}={avg_nodes:.0f}$"
        )

    finite_values = [
        value
        for values in cut_summary.values()
        for value in values
        if not np.isnan(value)
    ]
    if not finite_values:
        raise RuntimeError("No valid cut sizes found to plot.")

    sorted_values = sorted(finite_values)
    max_value = sorted_values[-1]
    lower_band_values = [value for value in finite_values if value < max_value]
    second_largest = max(lower_band_values) if lower_band_values else max_value
    lower_max = second_largest * 1.25
    high_values = [value for value in finite_values if value > lower_max * 1.4]
    upper_min = min(high_values) * 0.92 if high_values else lower_max * 1.5
    upper_max = max_value * 1.08

    ax_bottom.set_ylim(0, lower_max)
    ax_top.set_ylim(upper_min, upper_max)

    for ax in (ax_top, ax_bottom):
        ax.grid(axis="y", linestyle=(0, (5, 5)), alpha=0.35)
        ax.set_axisbelow(True)

    for index, (_, _, color, _) in enumerate(ALGORITHMS):
        values = [cut_summary[dataset][index] for dataset in dataset_names]
        counts = [count_summary[dataset][index] for dataset in dataset_names]
        x_positions = x + offsets[index]

        for xpos, value, count in zip(x_positions, values, counts):
            if np.isnan(value):
                continue
            target_ax = ax_top if value >= upper_min else ax_bottom
            target_ax.text(
                xpos,
                value + max(2.0, value * 0.025),
                f"{value:.1f}\n(n={count})",
                ha="center",
                va="bottom",
                fontsize=8,
                color=color,
            )

    ax_bottom.set_xticks(x)
    ax_bottom.set_xticklabels(tick_labels, fontsize=10)
    ax_bottom.set_ylabel("Mean Cut Size", fontsize=12)
    ax_top.set_title("Mean Solution Size Across Datasets", fontsize=14, pad=12)

    ax_top.spines["bottom"].set_visible(False)
    ax_bottom.spines["top"].set_visible(False)
    ax_top.tick_params(labeltop=False, bottom=False)
    ax_bottom.tick_params(top=False)

    break_marker = dict(marker=[(-1, -1), (1, 1)], markersize=10, linestyle="none", color="k", mec="k", mew=1, clip_on=False)
    ax_top.plot([0, 1], [0, 0], transform=ax_top.transAxes, **break_marker)
    ax_bottom.plot([0, 1], [1, 1], transform=ax_bottom.transAxes, **break_marker)

    handles, labels = ax_top.get_legend_handles_labels()
    fig.legend(
        handles,
        labels,
        loc="upper center",
        ncol=4,
        bbox_to_anchor=(0.5, 1.01),
        frameon=True,
        fancybox=False,
        edgecolor="black",
        fontsize=10,
    )

    note = (
        "Each point shows the mean cut size over valid runs only. "
        "Numbers above points show the mean cut size and the number of valid runs n. "
        "The broken y-axis separates the Large heuristic values from the rest."
    )
    fig.text(0.5, 0.01, note, ha="center", fontsize=10)
    fig.subplots_adjust(top=0.84, bottom=0.14)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=300, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    output_path = SCRIPT_DIR / "solution_size_summary.png"
    plot_solution_sizes(output_path)
    print(f"Saved plot to: {output_path}")


if __name__ == "__main__":
    main()
