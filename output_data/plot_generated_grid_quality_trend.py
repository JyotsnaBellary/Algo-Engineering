from __future__ import annotations

import csv
import re
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


SCRIPT_DIR = Path(__file__).resolve().parent
CSV_PATH = SCRIPT_DIR / "grid_output_server.csv"

INSTANCE_RE = re.compile(r"grid_(\d+)x(\d+)_k(\d+)_rep(\d+)")

ALGORITHMS = [
    ("Heuristic", "heuristic", "#C96A72", "o", "-"),
    ("Parallel Heuristic", "parallel_heuristic", "#B3E2CD", "s", "--"),
    ("Approximation", "approximation", "#CBD5E8", "^", "-."),
]


def parse_bool(value: str) -> bool:
    return value.strip() in {"1", "true", "True"}


def parse_float(value: str, default: float = -1.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def read_rows() -> list[dict[str, str]]:
    with CSV_PATH.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def summarize_by_grid_size() -> tuple[dict[str, list[tuple[int, float, int]]], list[int]]:
    rows = read_rows()
    ratios_by_algorithm: dict[str, dict[int, list[float]]] = {
        prefix: defaultdict(list) for _, prefix, _, _, _ in ALGORITHMS
    }

    for row in rows:
        match = INSTANCE_RE.fullmatch(row["instance_name"])
        if not match:
            continue

        r, c, _, _ = map(int, match.groups())
        num_nodes = r * c

        if not parse_bool(row.get("exact_valid", "0")):
            continue

        exact_cut = parse_float(row.get("exact_cut_size", "-1"))
        if exact_cut <= 0:
            continue

        for _, prefix, _, _, _ in ALGORITHMS:
            if not parse_bool(row.get(f"{prefix}_valid", "0")):
                continue
            alg_cut = parse_float(row.get(f"{prefix}_cut_size", "-1"))
            if alg_cut < 0:
                continue
            ratios_by_algorithm[prefix][num_nodes].append(alg_cut / exact_cut)

    summary: dict[str, list[tuple[int, float, int]]] = {}
    all_sizes = sorted(
        {
            size
            for per_algorithm in ratios_by_algorithm.values()
            for size in per_algorithm.keys()
        }
    )

    for _, prefix, _, _, _ in ALGORITHMS:
        entries = []
        for size in sorted(ratios_by_algorithm[prefix]):
            values = ratios_by_algorithm[prefix][size]
            entries.append((size, sum(values) / len(values), len(values)))
        summary[prefix] = entries

    return summary, all_sizes


def plot(output_path: Path) -> None:
    summary, all_sizes = summarize_by_grid_size()

    fig, ax = plt.subplots(figsize=(8.8, 5.6))

    for label, prefix, color, marker, linestyle in ALGORITHMS:
        entries = summary[prefix]
        xs = [size for size, _, _ in entries]
        ys = [ratio for _, ratio, _ in entries]
        counts = [count for _, _, count in entries]

        ax.plot(
            xs,
            ys,
            label=label,
            color=color,
            marker=marker,
            markersize=8,
            linewidth=2.0,
            linestyle=linestyle,
            markeredgecolor="black",
            markeredgewidth=0.8,
        )

        for x, y, n in zip(xs, ys, counts):
            ax.text(
                x,
                y + 0.03,
                f"{y:.3f}\n(n={n})",
                ha="center",
                va="bottom",
                fontsize=8,
                color=color,
            )

    ax.axhline(1.0, color="#666666", linestyle=(0, (4, 4)), linewidth=1.0)
    ax.text(all_sizes[-1] + 70, 1.0, "exact", ha="left", va="center", fontsize=9, color="#666666")

    ax.set_title("Generated Grid Graphs: Solution Quality Trend", fontsize=14, pad=12)
    ax.set_xlabel("Number of Nodes in Grid", fontsize=11)
    ax.set_ylabel("Mean Cut-Size Ratio to Exact", fontsize=11)
    ax.set_xticks(all_sizes)
    ax.set_ylim(0.95, 1.25)
    ax.grid(axis="y", linestyle=(0, (5, 5)), alpha=0.35)
    ax.set_axisbelow(True)

    ax.legend(
        loc="upper right",
        frameon=True,
        fancybox=False,
        edgecolor="black",
        fontsize=10,
    )

    fig.tight_layout()

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=300, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    output_path = SCRIPT_DIR / "generated_grid_quality_trend.png"
    plot(output_path)
    print(f"Saved plot to: {output_path}")


if __name__ == "__main__":
    main()
