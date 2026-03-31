from __future__ import annotations

import csv
import re
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt


SCRIPT_DIR = Path(__file__).resolve().parent

LOCAL_CSV = SCRIPT_DIR / "grid_output_local.csv"
SERVER_CSV = SCRIPT_DIR / "grid_output_server.csv"
OUTPUT_PATH = SCRIPT_DIR / "parallel_speedup_comparison.png"

INSTANCE_PATTERN = re.compile(r"^grid_(\d+)x(\d+)_k(\d+)_rep(\d+)$")

MACHINE_STYLES = {
    "Local": {"color": "#6C7A89", "marker": "o", "linestyle": "-"},
    "Server": {"color": "#C96A72", "marker": "s", "linestyle": "-"},
}


def parse_bool(value: str) -> bool:
    return str(value).strip() in {"1", "true", "True"}


def parse_float(value: str) -> float | None:
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def read_machine_rows(path: Path) -> dict[str, dict[str, object]]:
    rows: dict[str, dict[str, object]] = {}
    with path.open(newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            instance_name = row["instance_name"]
            match = INSTANCE_PATTERN.match(instance_name)
            if not match:
                continue

            rows[instance_name] = {
                "k": int(match.group(3)),
                "heuristic_valid": parse_bool(row.get("heuristic_valid", "0")),
                "parallel_valid": parse_bool(row.get("parallel_heuristic_valid", "0")),
                "heuristic_runtime": parse_float(row.get("heuristic_execution_time_ms", "")),
                "parallel_runtime": parse_float(row.get("parallel_heuristic_execution_time_ms", "")),
            }
    return rows


def build_speedup_summary() -> tuple[list[int], dict[str, list[float]], dict[str, list[int]]]:
    machine_data = {
        "Local": read_machine_rows(LOCAL_CSV),
        "Server": read_machine_rows(SERVER_CSV),
    }

    overlap_instances = sorted(set(machine_data["Local"]) & set(machine_data["Server"]))
    speedups_by_machine_and_k: dict[str, dict[int, list[float]]] = {
        "Local": defaultdict(list),
        "Server": defaultdict(list),
    }

    for machine_name, rows in machine_data.items():
        for instance_name in overlap_instances:
            row = rows[instance_name]
            heuristic_valid = bool(row["heuristic_valid"])
            parallel_valid = bool(row["parallel_valid"])
            heuristic_runtime = row["heuristic_runtime"]
            parallel_runtime = row["parallel_runtime"]

            if not heuristic_valid or not parallel_valid:
                continue
            if heuristic_runtime is None or parallel_runtime is None:
                continue
            if heuristic_runtime <= 0 or parallel_runtime <= 0:
                continue

            k = int(row["k"])
            speedup = heuristic_runtime / parallel_runtime
            speedups_by_machine_and_k[machine_name][k].append(speedup)

    k_values = sorted(
        {
            k
            for machine_values in speedups_by_machine_and_k.values()
            for k in machine_values.keys()
        }
    )

    mean_summary: dict[str, list[float]] = {}
    count_summary: dict[str, list[int]] = {}
    for machine_name in ("Local", "Server"):
        means: list[float] = []
        counts: list[int] = []
        for k in k_values:
            values = speedups_by_machine_and_k[machine_name].get(k, [])
            counts.append(len(values))
            means.append(sum(values) / len(values) if values else float("nan"))
        mean_summary[machine_name] = means
        count_summary[machine_name] = counts

    return k_values, mean_summary, count_summary


def plot_speedup_comparison(output_path: Path) -> None:
    k_values, mean_summary, count_summary = build_speedup_summary()

    fig, ax = plt.subplots(figsize=(8.8, 5.6))

    for machine_name in ("Local", "Server"):
        style = MACHINE_STYLES[machine_name]
        values = mean_summary[machine_name]
        counts = count_summary[machine_name]

        ax.plot(
            k_values,
            values,
            color=style["color"],
            marker=style["marker"],
            linestyle=style["linestyle"],
            linewidth=2.0,
            markersize=8,
            markerfacecolor=style["color"],
            markeredgecolor="black",
            markeredgewidth=0.8,
            label=machine_name,
        )

        for k, value, count in zip(k_values, values, counts):
            if value != value:
                continue
            ax.text(
                k,
                value + 0.06,
                f"{value:.2f}\n(n={count})",
                ha="center",
                va="bottom",
                fontsize=8,
                color=style["color"],
            )

    ax.axhline(1.0, color="#666666", linestyle=(0, (4, 4)), linewidth=1.1)
    ax.text(
        k_values[-1] + 1.2,
        1.0,
        "break-even",
        va="center",
        ha="left",
        fontsize=9,
        color="#666666",
    )

    ax.set_xticks(k_values)
    ax.set_xlim(min(k_values) - 1, max(k_values) + 4)
    ax.set_ylim(0, max(max(mean_summary["Local"]), max(mean_summary["Server"])) * 1.25)
    ax.set_xlabel("Terminal Count $k$", fontsize=11)
    ax.set_ylabel("Mean Speedup $T_{seq} / T_{par}$", fontsize=11)
    ax.set_title("Parallel Heuristic Speedup on Overlapping Generated Instances", fontsize=13, pad=10)
    ax.grid(axis="y", linestyle=(0, (5, 5)), alpha=0.35)
    ax.set_axisbelow(True)
    ax.legend(
        loc="upper left",
        frameon=True,
        fancybox=False,
        edgecolor="black",
        fontsize=10,
    )

    note = (
        "Speedup values above 1 indicate that the parallel heuristic is faster than the sequential heuristic."
    )
    fig.text(0.5, 0.01, note, ha="center", fontsize=9.5)
    fig.tight_layout(rect=(0, 0.06, 1, 1))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_path, dpi=300, bbox_inches="tight")
    plt.close(fig)


def main() -> None:
    plot_speedup_comparison(OUTPUT_PATH)
    print(f"Saved plot to: {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
