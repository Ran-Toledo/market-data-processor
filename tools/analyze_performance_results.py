import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path
import re


NUMERIC_FIELDS = [
    "durationSec",
    "workerCount",
    "producerCount",
    "activeProducerCount",
    "producerBurstSize",
    "producerSleepUs",
    "processingDelayUs",
    "busyWorkIterations",
    "workerQueueCapacity",
    "eventsGenerated",
    "eventsAccepted",
    "eventsRejected",
    "eventsProcessed",
    "queueDropped",
    "queueFailedEnqueue",
    "queueMaxDepthSeen",
    "queueMaxTotalDepthSampled",
    "queueCapacityTotal",
    "nearCapacitySamplePercent",
    "generatedPerSec",
    "acceptedPerSec",
    "processedPerSec",
    "latencyAverageNs",
    "latencyMaxNs",
    "latencyP50Ns",
    "latencyP95Ns",
    "latencyP99Ns",
]

SAMPLE_NUMERIC_FIELDS = [
    "run",
    "elapsedSec",
    "intervalSec",
    "generatedDelta",
    "acceptedDelta",
    "rejectedDelta",
    "processedDelta",
    "validDelta",
    "invalidDelta",
    "duplicateDelta",
    "outOfOrderDelta",
    "sequenceGapDelta",
    "queueDroppedDelta",
    "queueFailedEnqueueDelta",
    "generatedPerSec",
    "acceptedPerSec",
    "rejectedPerSec",
    "processedPerSec",
    "queueCurrentDepthTotal",
    "queueCapacityTotal",
    "queueMaxDepthSeen",
    "nearCapacityQueues",
    "queueDepthPercent",
    "latencySampleCount",
    "latencyP50Ns",
    "latencyP95Ns",
    "latencyP99Ns",
]


SUMMARY_FIELDS = [
    "profile",
    "runs",
    "workerCount",
    "producerCount",
    "activeProducerCount",
    "producerBurstSize",
    "producerSleepUs",
    "processingDelayUs",
    "busyWorkIterations",
    "workerQueueCapacity",
    "queueFullPolicy",
    "generatedPerSec_mean",
    "acceptedPerSec_mean",
    "processedPerSec_mean",
    "processedPerSec_min",
    "processedPerSec_max",
    "eventsRejected_mean",
    "queueDropped_mean",
    "queueMaxDepthSeen_mean",
    "nearCapacitySamplePercent_mean",
    "latencyP50Ns_mean",
    "latencyP95Ns_mean",
    "latencyP99Ns_mean",
]


def parse_args():
    parser = argparse.ArgumentParser(
        description="Aggregate and optionally plot pipeline load experiment results."
    )
    parser.add_argument("--input", required=True, help="Input CSV from pipeline_load_experiments.")
    parser.add_argument(
        "--samples-input",
        help="Optional interval samples CSV from pipeline_load_experiments.",
    )
    parser.add_argument(
        "--summary-out",
        default="results/performance-load-summary.csv",
        help="Output CSV containing one aggregate row per profile.",
    )
    parser.add_argument(
        "--plots-dir",
        help="Optional output directory for PNG plots. Requires matplotlib.",
    )
    parser.add_argument(
        "--timeseries-plots-dir",
        help="Optional output directory for per-profile interval plots. Requires matplotlib.",
    )
    return parser.parse_args()


def load_rows(path):
    with Path(path).open(newline="") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)

    for row in rows:
        for field in NUMERIC_FIELDS:
            if field in row and row[field] != "":
                row[field] = float(row[field])

    return rows


def load_sample_rows(path):
    if not path:
        return []

    input_path = Path(path)
    if not input_path.exists():
        raise SystemExit(f"Sample input CSV does not exist: {path}")

    with input_path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)

    for row in rows:
        for field in SAMPLE_NUMERIC_FIELDS:
            if field in row and row[field] != "":
                row[field] = float(row[field])

    return rows


def mean(rows, field):
    return statistics.fmean(row[field] for row in rows)


def summarize(rows):
    grouped = defaultdict(list)
    for row in rows:
        grouped[row["profile"]].append(row)

    summary_rows = []
    for profile in sorted(grouped):
        profile_rows = grouped[profile]
        first = profile_rows[0]
        processed = [row["processedPerSec"] for row in profile_rows]

        summary_rows.append(
            {
                "profile": profile,
                "runs": len(profile_rows),
                "workerCount": int(first["workerCount"]),
                "producerCount": int(first["producerCount"]),
                "activeProducerCount": int(first["activeProducerCount"]),
                "producerBurstSize": int(first["producerBurstSize"]),
                "producerSleepUs": int(first["producerSleepUs"]),
                "processingDelayUs": int(first["processingDelayUs"]),
                "busyWorkIterations": int(first["busyWorkIterations"]),
                "workerQueueCapacity": int(first["workerQueueCapacity"]),
                "queueFullPolicy": first["queueFullPolicy"],
                "generatedPerSec_mean": mean(profile_rows, "generatedPerSec"),
                "acceptedPerSec_mean": mean(profile_rows, "acceptedPerSec"),
                "processedPerSec_mean": mean(profile_rows, "processedPerSec"),
                "processedPerSec_min": min(processed),
                "processedPerSec_max": max(processed),
                "eventsRejected_mean": mean(profile_rows, "eventsRejected"),
                "queueDropped_mean": mean(profile_rows, "queueDropped"),
                "queueMaxDepthSeen_mean": mean(profile_rows, "queueMaxDepthSeen"),
                "nearCapacitySamplePercent_mean": mean(profile_rows, "nearCapacitySamplePercent"),
                "latencyP50Ns_mean": mean(profile_rows, "latencyP50Ns"),
                "latencyP95Ns_mean": mean(profile_rows, "latencyP95Ns"),
                "latencyP99Ns_mean": mean(profile_rows, "latencyP99Ns"),
            }
        )

    return summary_rows


def write_summary(path, summary_rows):
    output_path = Path(path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=SUMMARY_FIELDS)
        writer.writeheader()
        writer.writerows(summary_rows)


def plot_summary(summary_rows, plots_dir):
    if not plots_dir:
        return

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; wrote CSV summary only.")
        return

    output_dir = Path(plots_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    profiles = [row["profile"] for row in summary_rows]
    x = range(len(profiles))

    def save_bar_chart(field, title, ylabel, filename):
        values = [row[field] for row in summary_rows]
        plt.figure(figsize=(max(10, len(profiles) * 0.55), 6))
        plt.bar(x, values)
        plt.xticks(x, profiles, rotation=45, ha="right")
        plt.ylabel(ylabel)
        plt.title(title)
        plt.tight_layout()
        plt.savefig(output_dir / filename)
        plt.close()

    save_bar_chart("processedPerSec_mean", "Processed throughput by profile", "events/sec", "processed_throughput.png")
    save_bar_chart("generatedPerSec_mean", "Generated throughput by profile", "events/sec", "generated_throughput.png")
    save_bar_chart("nearCapacitySamplePercent_mean", "Queue near-capacity samples", "percent", "queue_near_capacity.png")
    save_bar_chart("latencyP99Ns_mean", "P99 latency by profile", "nanoseconds", "latency_p99.png")


def sanitize_filename(value):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", value).strip("_")


def plot_time_series(sample_rows, plots_dir):
    if not plots_dir or not sample_rows:
        return

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not installed; skipped time-series plots.")
        return

    output_dir = Path(plots_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    grouped = defaultdict(list)
    for row in sample_rows:
        grouped[(row["profile"], int(row["run"]))].append(row)

    for (profile, run), rows in sorted(grouped.items()):
        rows.sort(key=lambda row: row["elapsedSec"])
        elapsed = [row["elapsedSec"] for row in rows]

        fig, axes = plt.subplots(4, 1, figsize=(12, 12), sharex=True)
        fig.suptitle(f"{profile} run {run}")

        axes[0].plot(elapsed, [row["generatedPerSec"] for row in rows], label="generated/sec")
        axes[0].plot(elapsed, [row["acceptedPerSec"] for row in rows], label="accepted/sec")
        axes[0].plot(elapsed, [row["processedPerSec"] for row in rows], label="processed/sec")
        axes[0].set_ylabel("events/sec")
        axes[0].legend(loc="best")

        axes[1].plot(elapsed, [row["queueCurrentDepthTotal"] for row in rows], label="current depth")
        axes[1].plot(elapsed, [row["queueMaxDepthSeen"] for row in rows], label="max depth seen")
        axes[1].plot(elapsed, [row["queueCapacityTotal"] for row in rows], label="total capacity")
        axes[1].set_ylabel("queue entries")
        axes[1].legend(loc="best")

        axes[2].plot(elapsed, [row["latencyP50Ns"] for row in rows], label="p50")
        axes[2].plot(elapsed, [row["latencyP95Ns"] for row in rows], label="p95")
        axes[2].plot(elapsed, [row["latencyP99Ns"] for row in rows], label="p99")
        axes[2].set_ylabel("latency ns")
        axes[2].legend(loc="best")

        axes[3].plot(elapsed, [row["rejectedDelta"] for row in rows], label="rejected")
        axes[3].plot(elapsed, [row["queueDroppedDelta"] for row in rows], label="queue dropped")
        axes[3].plot(elapsed, [row["sequenceGapDelta"] for row in rows], label="sequence gaps")
        axes[3].plot(elapsed, [row["duplicateDelta"] for row in rows], label="duplicates")
        axes[3].set_ylabel("interval count")
        axes[3].set_xlabel("elapsed seconds")
        axes[3].legend(loc="best")

        for axis in axes:
            axis.grid(True, alpha=0.3)

        fig.tight_layout()
        filename = f"{sanitize_filename(profile)}_run_{run}_timeseries.png"
        fig.savefig(output_dir / filename)
        plt.close(fig)


def main():
    args = parse_args()
    rows = load_rows(args.input)
    if not rows:
        raise SystemExit("No rows found in input CSV.")

    summary_rows = summarize(rows)
    write_summary(args.summary_out, summary_rows)
    plot_summary(summary_rows, args.plots_dir)
    sample_rows = load_sample_rows(args.samples_input)
    plot_time_series(sample_rows, args.timeseries_plots_dir)
    print(f"Loaded {len(rows)} result rows across {len(summary_rows)} profiles from {args.input}")
    if sample_rows:
        sample_profiles = {(row["profile"], int(row["run"])) for row in sample_rows}
        print(
            f"Loaded {len(sample_rows)} interval samples across "
            f"{len(sample_profiles)} profile runs from {args.samples_input}"
        )
    if len(summary_rows) == 1:
        print("Only one profile was found in the input CSV; plots will contain one bar.")
    print(f"Wrote summary to {args.summary_out}")


if __name__ == "__main__":
    main()
