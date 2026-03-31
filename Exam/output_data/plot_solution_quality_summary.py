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
}

ALGORITHMS = [
    ("Heuristic", "heuristic", "#C96A72"),
    ("Parallel Heuristic", "parallel_heuristic", "#62B59F"),
    ("Approximation", "approximation", "#7B8FC6"),
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


def overlap_rows(rows: list[dict[str, str]], prefix: str) -> list[dict[str, str]]:
    overlap: list[dict[str, str]] = []
    for row in rows:
        if not parse_bool(row.get("exact_valid", "0")):
            continue
        if not parse_bool(row.get(f"{prefix}_valid", "0")):
            continue
        exact_cut = parse_float(row.get("exact_cut_size", "-1"))
        alg_cut = parse_float(row.get(f"{prefix}_cut_size", "-1"))
        if exact_cut <= 0 or alg_cut < 0:
            continue
        overlap.append(row)
    return overlap


def summarize_quality() -> tuple[dict[str, list[float]], dict[str, list[float]], dict[str, list[int]]]:
    ratio_summary: dict[str, list[float]] = {}
    match_summary: dict[str, list[float]] = {}
    overlap_summary: dict[str, list[int]] = {}

    for dataset_name in DATASET_FILES:
        rows = read_rows(resolve_csv(dataset_name))
        ratios_for_dataset: list[float] = []
        matches_for_dataset: list[float] = []
        overlaps_for_dataset: list[int] = []

        for _, prefix, _ in ALGORITHMS:
            overlap = overlap_rows(rows, prefix)
            overlaps_for_dataset.append(len(overlap))
            if not overlap:
                ratios_for_dataset.append(np.nan)
                matches_for_dataset.append(np.nan)
                continue

            ratios = []
            exact_matches = 0
            for row in overlap:
                exact_cut = parse_float(row["exact_cut_size"])
                alg_cut = parse_float(row[f"{prefix}_cut_size"])
                ratios.append(alg_cut / exact_cut)
                if abs(alg_cut - exact_cut) < 1e-9:
                    exact_matches += 1

            ratios_for_dataset.append(sum(ratios) / len(ratios))
            matches_for_dataset.append(100.0 * exact_matches / len(overlap))

        ratio_summary[dataset_name] = ratios_for_dataset
        match_summary[dataset_name] = matches_for_dataset
        overlap_summary[dataset_name] = overlaps_for_dataset

    return ratio_summary, match_summary, overlap_summary


def plot_quality_summary(output_path: Path) -> None:
    ratio_summary, _, overlap_summary = summarize_quality()
    dataset_names = list(DATASET_FILES.keys())

    x = np.arange(len(dataset_names))
    offsets = [-0.14, 0.0, 0.14]

    fig, ax = plt.subplots(figsize=(8.6, 5.6))

    for index, (label, _, color) in enumerate(ALGORITHMS):
        ratio_values = [ratio_summary[dataset][index] for dataset in dataset_names]
        overlap_values = [overlap_summary[dataset][index] for dataset in dataset_names]

        x_positions = x + offsets[index]
        ax.plot(
            x_positions,
            ratio_values,
            linestyle="None",
            marker="o",
            markersize=10,
            markerfacecolor=color,
            markeredgecolor="black",
            markeredgewidth=0.9,
            label=label,
            zorder=3,
        )

        for xpos, value, overlap_count in zip(x_positions, ratio_values, overlap_values):
            if np.isnan(value):
                continue
            ax.text(
                xpos,
                value + 0.03,
                f"{value:.3f}\n(n={overlap_count})",
                ha="center",
                va="bottom",
                fontsize=8,
                color=color,
            )

    ax.axhline(1.0, color="#666666", linestyle=(0, (4, 4)), linewidth=1.0)
    ax.text(
        x[-1] + 0.45,
        1.0,
        "exact",
        va="center",
        ha="left",
        fontsize=9,
        color="#666666",
    )
    ax.set_xticks(x)
    ax.set_xticklabels(dataset_names, fontsize=11)
    ax.set_ylim(0, 2.25)
    ax.set_xlim(-0.35, len(dataset_names) - 1 + 0.35)
    ax.set_ylabel("Mean Cut-Size Ratio to Exact", fontsize=11)
    ax.set_title("Solution Quality Relative to Exact", fontsize=13, pad=10)
    ax.grid(axis="y", linestyle=(0, (5, 5)), alpha=0.35)
    ax.set_axisbelow(True)

    handles, labels = ax.get_legend_handles_labels()
    fig.legend(
        handles,
        labels,
        loc="upper center",
        ncol=3,
        bbox_to_anchor=(0.5, 1.02),
        frameon=True,
        fancybox=False,
        edgecolor="black",
        fontsize=10,
    )

    note = (
        "Numbers above points show the mean ratio and the overlap size n."
    )
    fig.text(0.5, 0.01, note, ha="center", fontsize=10)
    fig.tight_layout(rect=(0, 0.06, 1, 0.93))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=300, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    output_path = SCRIPT_DIR / "solution_quality_summary.png"
    plot_quality_summary(output_path)
    print(f"Saved plot to: {output_path}")


if __name__ == "__main__":
    main()
