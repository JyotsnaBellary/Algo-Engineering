from __future__ import annotations

import csv
import re
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


SCRIPT_DIR = Path(__file__).resolve().parent
CSV_PATH = SCRIPT_DIR / "grid_output.csv"
INSTANCE_RE = re.compile(r"grid_(\d+)x(\d+)_k(\d+)_rep(\d+)")

ALGORITHMS = [
    ("Heuristic", "heuristic", "#C96A72", "o", "-"),
    ("Parallel Heuristic", "parallel_heuristic", "#B3E2CD", "s", "--"),
    ("Approximation", "approximation", "#CBD5E8", "^", "-."),
    ("Exact", "exact", "#FDCDAC", "D", ":"),
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


def summarize_by_terminal_count() -> dict[str, list[tuple[int, float, int]]]:
    rows = read_rows()
    cut_sizes_by_algorithm: dict[str, dict[int, list[float]]] = {
        prefix: defaultdict(list) for _, prefix, _, _, _ in ALGORITHMS
    }

    for row in rows:
        match = INSTANCE_RE.fullmatch(row["instance_name"])
        if not match:
            continue

        k = int(match.group(3))

        for _, prefix, _, _, _ in ALGORITHMS:
            if not parse_bool(row.get(f"{prefix}_valid", "0")):
                continue
            cut_size = parse_float(row.get(f"{prefix}_cut_size", "-1"))
            if cut_size < 0:
                continue
            cut_sizes_by_algorithm[prefix][k].append(cut_size)

    summary: dict[str, list[tuple[int, float, int]]] = {}
    for _, prefix, _, _, _ in ALGORITHMS:
        entries = []
        for k in sorted(cut_sizes_by_algorithm[prefix]):
            values = cut_sizes_by_algorithm[prefix][k]
            entries.append((k, sum(values) / len(values), len(values)))
        summary[prefix] = entries
    return summary


def plot(output_path: Path) -> None:
    summary = summarize_by_terminal_count()

    fig, ax = plt.subplots(figsize=(8.8, 5.6))

    for label, prefix, color, marker, linestyle in ALGORITHMS:
        entries = summary[prefix]
        xs = [k for k, _, _ in entries]
        ys = [mean_cut for _, mean_cut, _ in entries]

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

    ax.set_title("Generated Grid Graphs: Mean Solution Size vs Terminal Count", fontsize=14, pad=12)
    ax.set_xlabel("Number of Terminals $k$", fontsize=11)
    ax.set_ylabel("Mean Cut Size", fontsize=11)
    ax.grid(axis="y", linestyle=(0, (5, 5)), alpha=0.35)
    ax.set_axisbelow(True)
    ax.legend(
        loc="upper left",
        frameon=True,
        fancybox=False,
        edgecolor="black",
        fontsize=10,
    )

    note = "Only valid cuts on generated grid instances are included."
    fig.text(0.5, 0.01, note, ha="center", fontsize=10)
    fig.tight_layout(rect=(0, 0.06, 1, 1))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=300, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    output_path = SCRIPT_DIR / "generated_grid_cutsize_trend.png"
    plot(output_path)
    print(f"Saved plot to: {output_path}")


if __name__ == "__main__":
    main()
