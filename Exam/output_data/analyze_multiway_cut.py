#!/usr/bin/env python3
"""
Multiway Cut Benchmark Analysis

What this script does
---------------------
1. Reads a benchmark CSV file.
2. Optionally infers a size group (small / medium / large).
3. Computes summary tables:
   - successful runs per algorithm
   - timed out runs per algorithm
   - valid solutions per algorithm
   - skipped instances (optional, if you provide instance directories)
4. Generates plots for:
   - runtime comparison
   - cut size comparison
   - solution quality ratios
   - cut-to-node ratios
   - success / timeout counts
5. Writes all plots and summary CSV files to an output directory.

Usage examples
--------------
python analyze_multiway_cut.py --csv results.csv

python analyze_multiway_cut.py --csv results.csv --output analysis_out

python analyze_multiway_cut.py --csv results.csv \
    --instance-dir ./Track1/small \
    --instance-dir ./Track1/medium \
    --instance-dir ./Track1/large \
    --instance-dir ./Track2/small \
    --instance-dir ./Track2/medium \
    --instance-dir ./Track2/large \
    --instance-dir ./Track3/small \
    --instance-dir ./Track3/medium \
    --instance-dir ./Track3/large

Optional:
- If your CSV already has a size column, name it one of:
    size, graph_size, size_category
- Otherwise this script tries to infer size from:
    instance_name, track_name
  by searching for the substrings small / medium / large.
"""

import argparse
from pathlib import Path
import re
import math

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt


ALGORITHMS = {
    "heuristic": {
        "time": "heuristic_execution_time_ms",
        "cut": "heuristic_cut_size",
        "valid": "heuristic_valid",
        "timed_out": "heuristic_timed_out",
        "ratio": "heuristic_exact_ratio",
        "cut_to_node_ratio": "cut_to_node_heuristic_ratio",
    },
    "parallel_heuristic": {
        "time": "parallel_heuristic_execution_time_ms",
        "cut": "parallel_heuristic_cut_size",
        "valid": "parallel_heuristic_valid",
        "timed_out": "parallel_heuristic_timed_out",
        "ratio": "parallel_heuristic_exact_ratio",
        "cut_to_node_ratio": "cut_to_node_parallel_heuristic_ratio",
    },
    "approximation": {
        "time": "approximation_execution_time_ms",
        "cut": "approximation_cut_size",
        "valid": "approximation_valid",
        "timed_out": "approximation_timed_out",
        "ratio": "approximation_exact_ratio",
        "cut_to_node_ratio": "cut_to_node_approximation_ratio",
    },
    "exact": {
        # For exact we use best cut time, as requested.
        "time": "exact_best_time",
        "cut": "exact_cut_size",
        "valid": "exact_valid",
        "timed_out": "exact_timed_out",
        "ratio": None,
        "cut_to_node_ratio": "cut_to_node_exact_ratio",
    },
}


def parse_args():
    parser = argparse.ArgumentParser(description="Analyze Multiway Cut benchmark CSV.")
    parser.add_argument("--csv", required=True, help="Path to benchmark CSV file.")
    parser.add_argument("--output", default="multiway_cut_analysis", help="Output directory.")
    parser.add_argument(
        "--instance-dir",
        action="append",
        default=[],
        help="Directory containing .gr instances. Can be passed multiple times.",
    )
    parser.add_argument(
        "--instance-extension",
        default=".gr",
        help="Instance file extension for skipped-instance counting (default: .gr).",
    )
    parser.add_argument(
        "--save-cleaned-csv",
        action="store_true",
        help="Save the cleaned/enriched dataframe as CSV.",
    )
    return parser.parse_args()


def normalize_columns(df: pd.DataFrame) -> pd.DataFrame:
    df = df.copy()
    df.columns = [c.strip() for c in df.columns]
    return df


def infer_size_group(row: pd.Series) -> str:
    for col in ["size", "graph_size", "size_category"]:
        if col in row and pd.notna(row[col]):
            value = str(row[col]).strip().lower()
            if value in {"small", "medium", "large"}:
                return value

    text_parts = []
    for col in ["instance_name", "track_name"]:
        if col in row and pd.notna(row[col]):
            text_parts.append(str(row[col]).lower())
    text = " ".join(text_parts)

    for label in ["small", "medium", "large"]:
        if label in text:
            return label

    return "unknown"


def convert_numeric_columns(df: pd.DataFrame) -> pd.DataFrame:
    df = df.copy()
    for col in df.columns:
        if col in {"instance_name", "track_name", "size_group"}:
            continue
        df[col] = pd.to_numeric(df[col], errors="ignore")
    return df


def prepare_dataframe(csv_path: str) -> pd.DataFrame:
    df = pd.read_csv(csv_path)
    df = normalize_columns(df)
    df["size_group"] = df.apply(infer_size_group, axis=1)

    # Normalize track names if missing.
    if "track_name" not in df.columns:
        df["track_name"] = "unknown_track"

    df = convert_numeric_columns(df)
    return df


def make_output_dir(output_dir: str) -> Path:
    out = Path(output_dir)
    out.mkdir(parents=True, exist_ok=True)
    return out


def safe_mean(series: pd.Series):
    series = pd.to_numeric(series, errors="coerce").dropna()
    if len(series) == 0:
        return np.nan
    return series.mean()


def safe_median(series: pd.Series):
    series = pd.to_numeric(series, errors="coerce").dropna()
    if len(series) == 0:
        return np.nan
    return series.median()


def safe_count(series: pd.Series):
    return pd.to_numeric(series, errors="coerce").notna().sum()


def count_successful_runs(df: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for alg, cols in ALGORITHMS.items():
        valid_col = cols["valid"]
        timeout_col = cols["timed_out"]

        for (track, size), group in df.groupby(["track_name", "size_group"], dropna=False):
            total_rows = len(group)
            valid = int(group[valid_col].fillna(0).astype(int).sum()) if valid_col in group.columns else 0
            timed_out = int(group[timeout_col].fillna(0).astype(int).sum()) if timeout_col in group.columns else 0

            # "Successful" = present in CSV, valid, and not timed out.
            successful = int(
                ((group[valid_col].fillna(0).astype(int) == 1) &
                 (group[timeout_col].fillna(0).astype(int) == 0)).sum()
            ) if valid_col in group.columns and timeout_col in group.columns else 0

            rows.append({
                "track_name": track,
                "size_group": size,
                "algorithm": alg,
                "rows_in_csv": total_rows,
                "successful_runs": successful,
                "valid_runs": valid,
                "timed_out_runs": timed_out,
            })
    return pd.DataFrame(rows)


def collect_expected_instances(instance_dirs, extension=".gr") -> pd.DataFrame:
    records = []
    ext = extension.lower()

    for d in instance_dirs:
        p = Path(d)
        if not p.exists() or not p.is_dir():
            print(f"Warning: instance directory not found or not a directory: {p}")
            continue

        parts = [part.lower() for part in p.parts]
        size = "unknown"
        for label in ["small", "medium", "large"]:
            if label in parts or label in p.name.lower():
                size = label
                break

        track = p.parent.name if p.parent != p else "unknown_track"

        for file in p.rglob(f"*{ext}"):
            records.append({
                "instance_name": file.name,
                "track_name": track,
                "size_group": size,
                "full_path": str(file),
            })

    return pd.DataFrame(records)


def compute_skipped_instances(df: pd.DataFrame, instance_dirs, extension=".gr") -> pd.DataFrame:
    if not instance_dirs:
        return pd.DataFrame(columns=[
            "track_name", "size_group", "expected_instances",
            "executed_instances", "skipped_instances"
        ])

    expected = collect_expected_instances(instance_dirs, extension=extension)

    if expected.empty:
        return pd.DataFrame(columns=[
            "track_name", "size_group", "expected_instances",
            "executed_instances", "skipped_instances"
        ])

    executed = df[["instance_name", "track_name", "size_group"]].drop_duplicates()

    merged = expected.merge(
        executed.assign(executed=1),
        on=["instance_name", "track_name", "size_group"],
        how="left"
    )
    merged["executed"] = merged["executed"].fillna(0).astype(int)
    merged["skipped"] = 1 - merged["executed"]

    summary = (
        merged.groupby(["track_name", "size_group"], dropna=False)
        .agg(
            expected_instances=("instance_name", "count"),
            executed_instances=("executed", "sum"),
            skipped_instances=("skipped", "sum"),
        )
        .reset_index()
    )

    return summary


def summary_by_group(df: pd.DataFrame) -> pd.DataFrame:
    rows = []
    for (track, size), group in df.groupby(["track_name", "size_group"], dropna=False):
        row = {
            "track_name": track,
            "size_group": size,
            "num_instances": len(group),
            "avg_num_nodes": safe_mean(group["num_nodes"]) if "num_nodes" in group else np.nan,
            "avg_num_edges": safe_mean(group["num_edges"]) if "num_edges" in group else np.nan,
            "avg_num_terminals": safe_mean(group["num_terminals"]) if "num_terminals" in group else np.nan,
            "avg_graph_density": safe_mean(group["graph_density"]) if "graph_density" in group else np.nan,
        }

        for alg, cols in ALGORITHMS.items():
            row[f"{alg}_avg_runtime_ms"] = safe_mean(group[cols["time"]]) if cols["time"] in group else np.nan
            row[f"{alg}_median_runtime_ms"] = safe_median(group[cols["time"]]) if cols["time"] in group else np.nan
            row[f"{alg}_avg_cut_size"] = safe_mean(group[cols["cut"]]) if cols["cut"] in group else np.nan
            row[f"{alg}_median_cut_size"] = safe_median(group[cols["cut"]]) if cols["cut"] in group else np.nan

            if cols["ratio"] and cols["ratio"] in group:
                row[f"{alg}_avg_exact_ratio"] = safe_mean(group[cols["ratio"]])
                row[f"{alg}_median_exact_ratio"] = safe_median(group[cols["ratio"]])

            if cols["cut_to_node_ratio"] and cols["cut_to_node_ratio"] in group:
                row[f"{alg}_avg_cut_to_node_ratio"] = safe_mean(group[cols["cut_to_node_ratio"]])

        rows.append(row)

    return pd.DataFrame(rows)


def plot_grouped_bar(summary_df, value_cols, title, ylabel, output_path, rotate_xticks=False):
    if summary_df.empty:
        return

    labels = summary_df.apply(lambda r: f"{r['track_name']}\n{r['size_group']}", axis=1).tolist()
    x = np.arange(len(labels))
    width = 0.18 if len(value_cols) >= 4 else 0.25

    plt.figure(figsize=(max(10, len(labels) * 1.3), 6))
    for i, (col, label) in enumerate(value_cols):
        values = pd.to_numeric(summary_df[col], errors="coerce").fillna(0).to_numpy()
        plt.bar(x + (i - (len(value_cols) - 1) / 2) * width, values, width=width, label=label)

    plt.xticks(x, labels, rotation=30 if rotate_xticks else 0, ha="right" if rotate_xticks else "center")
    plt.ylabel(ylabel)
    plt.title(title)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path, dpi=200)
    plt.close()


def plot_boxplot_runtime(df: pd.DataFrame, output_path: Path):
    data = []
    labels = []

    for alg, cols in ALGORITHMS.items():
        if cols["time"] not in df.columns:
            continue
        series = pd.to_numeric(df[cols["time"]], errors="coerce").dropna()
        if len(series) > 0:
            data.append(series)
            labels.append(alg)

    if not data:
        return

    plt.figure(figsize=(8, 6))
    plt.boxplot(data, labels=labels)
    plt.ylabel("Runtime (ms)")
    plt.title("Runtime Distribution Across All Instances")
    plt.tight_layout()
    plt.savefig(output_path, dpi=200)
    plt.close()


def plot_scatter_nodes_vs_runtime(df: pd.DataFrame, output_dir: Path):
    if "num_nodes" not in df.columns:
        return

    for alg, cols in ALGORITHMS.items():
        if cols["time"] not in df.columns:
            continue

        x = pd.to_numeric(df["num_nodes"], errors="coerce")
        y = pd.to_numeric(df[cols["time"]], errors="coerce")

        mask = x.notna() & y.notna()
        if mask.sum() == 0:
            continue

        plt.figure(figsize=(8, 6))
        plt.scatter(x[mask], y[mask], alpha=0.7)
        plt.xlabel("Number of nodes")
        plt.ylabel("Runtime (ms)")
        plt.title(f"{alg}: Number of Nodes vs Runtime")
        plt.tight_layout()
        plt.savefig(output_dir / f"scatter_nodes_vs_runtime_{alg}.png", dpi=200)
        plt.close()


def plot_scatter_edges_vs_runtime(df: pd.DataFrame, output_dir: Path):
    if "num_edges" not in df.columns:
        return

    for alg, cols in ALGORITHMS.items():
        if cols["time"] not in df.columns:
            continue

        x = pd.to_numeric(df["num_edges"], errors="coerce")
        y = pd.to_numeric(df[cols["time"]], errors="coerce")

        mask = x.notna() & y.notna()
        if mask.sum() == 0:
            continue

        plt.figure(figsize=(8, 6))
        plt.scatter(x[mask], y[mask], alpha=0.7)
        plt.xlabel("Number of edges")
        plt.ylabel("Runtime (ms)")
        plt.title(f"{alg}: Number of Edges vs Runtime")
        plt.tight_layout()
        plt.savefig(output_dir / f"scatter_edges_vs_runtime_{alg}.png", dpi=200)
        plt.close()


def plot_hist_solution_quality(df: pd.DataFrame, output_dir: Path):
    for alg in ["heuristic", "parallel_heuristic", "approximation"]:
        ratio_col = ALGORITHMS[alg]["ratio"]
        if ratio_col not in df.columns:
            continue

        series = pd.to_numeric(df[ratio_col], errors="coerce").dropna()
        if len(series) == 0:
            continue

        plt.figure(figsize=(8, 6))
        plt.hist(series, bins=20)
        plt.xlabel("Approximation ratio against exact")
        plt.ylabel("Frequency")
        plt.title(f"{alg}: Solution Quality Distribution")
        plt.tight_layout()
        plt.savefig(output_dir / f"solution_quality_hist_{alg}.png", dpi=200)
        plt.close()


def plot_line_by_size(summary_df: pd.DataFrame, metric_prefix: str, ylabel: str, title: str, output_path: Path):
    size_order = ["small", "medium", "large", "unknown"]
    size_map = {s: i for i, s in enumerate(size_order)}

    df = summary_df.copy()
    df["size_order"] = df["size_group"].map(size_map).fillna(99)
    df = df.sort_values(["track_name", "size_order"])

    plt.figure(figsize=(9, 6))
    for alg in ALGORITHMS.keys():
        col = f"{alg}_{metric_prefix}"
        if col not in df.columns:
            continue

        temp = (
            df.groupby("size_group", dropna=False)[col]
            .mean()
            .reindex(size_order)
        )
        temp = temp.dropna()
        if len(temp) == 0:
            continue
        plt.plot(temp.index, temp.values, marker="o", label=alg)

    plt.ylabel(ylabel)
    plt.title(title)
    plt.legend()
    plt.tight_layout()
    plt.savefig(output_path, dpi=200)
    plt.close()


def save_table(df: pd.DataFrame, path: Path):
    df.to_csv(path, index=False)


def main():
    args = parse_args()
    out_dir = make_output_dir(args.output)

    df = prepare_dataframe(args.csv)

    if args.save_cleaned_csv:
        df.to_csv(out_dir / "cleaned_input.csv", index=False)

    summary = summary_by_group(df)
    success_summary = count_successful_runs(df)
    skipped_summary = compute_skipped_instances(df, args.instance_dir, args.instance_extension)

    save_table(summary, out_dir / "summary_by_track_and_size.csv")
    save_table(success_summary, out_dir / "success_timeout_valid_summary.csv")
    save_table(skipped_summary, out_dir / "skipped_instance_summary.csv")

    # 1. Average runtime comparison
    runtime_cols = [(f"{alg}_avg_runtime_ms", alg) for alg in ALGORITHMS.keys() if f"{alg}_avg_runtime_ms" in summary.columns]
    plot_grouped_bar(
        summary,
        runtime_cols,
        "Average Runtime Comparison by Track and Size",
        "Average runtime (ms)",
        out_dir / "avg_runtime_comparison.png",
        rotate_xticks=True,
    )

    # 2. Median runtime comparison
    median_runtime_cols = [(f"{alg}_median_runtime_ms", alg) for alg in ALGORITHMS.keys() if f"{alg}_median_runtime_ms" in summary.columns]
    plot_grouped_bar(
        summary,
        median_runtime_cols,
        "Median Runtime Comparison by Track and Size",
        "Median runtime (ms)",
        out_dir / "median_runtime_comparison.png",
        rotate_xticks=True,
    )

    # 3. Average cut size comparison
    cut_cols = [(f"{alg}_avg_cut_size", alg) for alg in ALGORITHMS.keys() if f"{alg}_avg_cut_size" in summary.columns]
    plot_grouped_bar(
        summary,
        cut_cols,
        "Average Cut Size Comparison by Track and Size",
        "Average cut size",
        out_dir / "avg_cut_size_comparison.png",
        rotate_xticks=True,
    )

    # 4. Average exact-ratio comparison
    ratio_cols = []
    for alg in ["heuristic", "parallel_heuristic", "approximation"]:
        col = f"{alg}_avg_exact_ratio"
        if col in summary.columns:
            ratio_cols.append((col, alg))

    if ratio_cols:
        plot_grouped_bar(
            summary,
            ratio_cols,
            "Average Solution Quality Ratio vs Exact",
            "Average ratio",
            out_dir / "avg_solution_quality_ratio.png",
            rotate_xticks=True,
        )

    # 5. Cut-to-node ratio comparison
    cut_to_node_cols = []
    for alg in ALGORITHMS.keys():
        col = f"{alg}_avg_cut_to_node_ratio"
        if col in summary.columns:
            cut_to_node_cols.append((col, alg))

    if cut_to_node_cols:
        plot_grouped_bar(
            summary,
            cut_to_node_cols,
            "Average Cut-to-Node Ratio Comparison",
            "Average cut / nodes",
            out_dir / "avg_cut_to_node_ratio.png",
            rotate_xticks=True,
        )

    # 6. Success counts
    if not success_summary.empty:
        pivot_success = success_summary.pivot_table(
            index=["track_name", "size_group"],
            columns="algorithm",
            values="successful_runs",
            fill_value=0
        ).reset_index()
        value_cols = [(alg, alg) for alg in pivot_success.columns if alg not in ["track_name", "size_group"]]
        plot_grouped_bar(
            pivot_success,
            value_cols,
            "Successful Runs per Algorithm",
            "Successful runs",
            out_dir / "successful_runs.png",
            rotate_xticks=True,
        )

        pivot_timeout = success_summary.pivot_table(
            index=["track_name", "size_group"],
            columns="algorithm",
            values="timed_out_runs",
            fill_value=0
        ).reset_index()
        value_cols = [(alg, alg) for alg in pivot_timeout.columns if alg not in ["track_name", "size_group"]]
        plot_grouped_bar(
            pivot_timeout,
            value_cols,
            "Timed-Out Runs per Algorithm",
            "Timed-out runs",
            out_dir / "timed_out_runs.png",
            rotate_xticks=True,
        )

    # 7. Skipped instance counts
    if not skipped_summary.empty:
        plot_grouped_bar(
            skipped_summary,
            [("expected_instances", "expected"), ("executed_instances", "executed"), ("skipped_instances", "skipped")],
            "Expected vs Executed vs Skipped Instances",
            "Count",
            out_dir / "expected_executed_skipped.png",
            rotate_xticks=True,
        )

    # 8. Runtime boxplot
    plot_boxplot_runtime(df, out_dir / "runtime_distribution_boxplot.png")

    # 9. Scatter plots
    plot_scatter_nodes_vs_runtime(df, out_dir)
    plot_scatter_edges_vs_runtime(df, out_dir)

    # 10. Solution quality histograms
    plot_hist_solution_quality(df, out_dir)

    # 11. Trend by size group
    plot_line_by_size(
        summary,
        metric_prefix="avg_runtime_ms",
        ylabel="Average runtime (ms)",
        title="Runtime Trend Across Size Groups",
        output_path=out_dir / "runtime_trend_by_size.png",
    )
    plot_line_by_size(
        summary,
        metric_prefix="avg_cut_size",
        ylabel="Average cut size",
        title="Cut Size Trend Across Size Groups",
        output_path=out_dir / "cut_size_trend_by_size.png",
    )

    # 12. Overall summary text file
    with open(out_dir / "report_notes.txt", "w", encoding="utf-8") as f:
        f.write("Suggested points to report:\n")
        f.write("1. Runtime comparison between heuristic, parallel heuristic, approximation, and exact (using exact_best_time).\n")
        f.write("2. Cut size comparison across algorithms.\n")
        f.write("3. Solution quality using heuristic_exact_ratio, parallel_heuristic_exact_ratio, approximation_exact_ratio.\n")
        f.write("4. Cut-to-node ratio as a normalized cut measure across different graph sizes.\n")
        f.write("5. Successful runs, timed-out runs, and optionally skipped instances.\n")
        f.write("6. Scaling behavior: runtime vs number of nodes / edges.\n")
        f.write("7. Median runtime in addition to mean runtime, to reduce outlier effects.\n")
        f.write("8. Whether parallel heuristic provides a real runtime benefit over heuristic.\n")
        f.write("9. Whether approximation is closer to exact than the heuristic variants.\n")
        f.write("10. Whether denser graphs or more terminals lead to larger cuts and longer runtimes.\n")

    print(f"Analysis complete. Output written to: {out_dir.resolve()}")


if __name__ == "__main__":
    main()
