#!/usr/bin/env python3
"""Runs Carbon's benchmarks and prints their results as the tables of Docs/Optimizations.md.

    python Scripts/Benchmarks.py --out Scratch/Benchmarks.json            run everything, median of 3 repetitions
    python Scripts/Benchmarks.py --filter "Rows/" --out Scratch/Rows.json   run the benchmarks matching a regex
    python Scripts/Benchmarks.py --table v1.json v2.json                  print saved results side by side

CarbonBenchmarks comes from --build (default: Build/Release). Use a Release build; every number is the median of
the repetitions. The saved file holds one entry per benchmark with its time and counters.
"""

import argparse
import json
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent

# The counters of a frame benchmark, in the order of the table's columns, with their units.
COUNTERS = [
    ("p95_us", "p95", "us"),
    ("allocs", "allocs/frame", ""),
    ("vertices", "vertices", ""),
    ("indices", "indices", ""),
    ("primitives", "primitives", ""),
    ("commands", "commands", ""),
    ("draw_bytes", "draw bytes", "B"),
    ("atlas_bytes", "atlas B/frame", "B"),
    ("atlas_full_per_1k", "atlas clears/1k frames", ""),
    ("frames_per_second", "frames/s", ""),
    ("frames_per_scroll", "frames/scroll", ""),
]


def find_executable(build):
    for candidate in (build / "Benchmarks" / "CarbonBenchmarks.exe", build / "Benchmarks" / "CarbonBenchmarks"):
        if candidate.exists():
            return candidate
    sys.exit(f"CarbonBenchmarks was not found below {build}; build the CarbonBenchmarks target first")


def run(arguments):
    executable = find_executable(pathlib.Path(arguments.build))
    with tempfile.TemporaryDirectory() as directory:
        raw = pathlib.Path(directory) / "raw.json"
        command = [
            str(executable),
            f"--benchmark_repetitions={arguments.repetitions}",
            "--benchmark_report_aggregates_only=true",
            "--benchmark_counters_tabular=true",
            f"--benchmark_out={raw}",
            "--benchmark_out_format=json",
        ]
        if arguments.filter:
            command.append(f"--benchmark_filter={arguments.filter}")
        subprocess.run(command, check=True)
        report = json.loads(raw.read_text(encoding="utf-8"))

    results = {}
    for entry in report["benchmarks"]:
        if entry.get("aggregate_name") != "median":
            continue
        name = entry["run_name"].replace("/manual_time", "")
        result = {"time_us": to_microseconds(entry["real_time"], entry["time_unit"])}
        if "error_message" in entry:
            result["skipped"] = entry["error_message"]
        for key, _, _ in COUNTERS + [("p50_us", "", "")]:
            if key in entry:
                result[key] = entry[key]
        results[name] = result
    # A skipped benchmark has no aggregates; keep it with its reason.
    for entry in report["benchmarks"]:
        name = entry.get("run_name", entry["name"]).replace("/manual_time", "")
        if entry.get("error_occurred") and name not in results:
            results[name] = {"skipped": entry.get("error_message", "skipped")}

    output = {"context": report["context"], "repetitions": arguments.repetitions, "benchmarks": results}
    out = pathlib.Path(arguments.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(output, indent=1), encoding="utf-8")
    print(f"Saved {len(results)} results to {out}")


def to_microseconds(value, unit):
    return value * {"ns": 1.0e-3, "us": 1.0, "ms": 1.0e3, "s": 1.0e6}[unit]


def format_time(microseconds):
    if microseconds >= 1.0e6:
        return f"{microseconds / 1.0e6:.2f} s"
    if microseconds >= 1000.0:
        return f"{microseconds / 1000.0:.3g} ms"
    return f"{microseconds:.3g} us"


def format_count(value, unit):
    if unit == "us":
        return format_time(value)
    text = f"{value:,.0f}" if abs(value) >= 100 or value == int(value) else f"{value:.3g}"
    return f"{text} {unit}".strip()


def format_change(first, last):
    if first is None or last is None:
        return ""
    if first == 0:
        return "=" if last == 0 else "new"
    change = (last - first) / first * 100.0
    if abs(change) < 0.5:
        return "="
    return f"{change:+.0f}%".replace("-", "−")


def table(arguments):
    versions = [json.loads(pathlib.Path(path).read_text(encoding="utf-8"))["benchmarks"] for path in arguments.table]
    names = []
    for version in versions:
        for name in version:
            if name not in names:
                names.append(name)
    header = ["Metric", "Case"] + [f"v{i + 1}" for i in range(len(versions))] + ["Change"]
    print("| " + " | ".join(header) + " |")
    print("| " + " | ".join("---" for _ in header) + " |")
    for name in names:
        entries = [version.get(name) for version in versions]
        if all(entry is None or "skipped" in entry for entry in entries):
            reason = next(entry["skipped"] for entry in entries if entry is not None)
            print(f"| Skipped | {name} | {reason} |")
            continue
        rows = [("time_us", "Frame time (median)", "us")] + [(key, label, unit) for key, label, unit in COUNTERS]
        for key, label, unit in rows:
            values = [entry.get(key) if entry is not None else None for entry in entries]
            if all(value is None for value in values):
                continue
            # Counters that are zero everywhere say nothing.
            if key != "time_us" and not arguments.all and all(not value for value in values):
                continue
            cells = [format_count(value, unit) if value is not None else "" for value in values]
            print(f"| {label} | {name} | " + " | ".join(cells) + f" | {format_change(values[0], values[-1])} |")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", default=str(ROOT / "Build" / "Release"), help="the build directory")
    parser.add_argument("--filter", default="", help="run only the benchmarks matching this regular expression")
    parser.add_argument("--repetitions", type=int, default=3, help="runs per benchmark; the median is reported")
    parser.add_argument("--out", help="where the results are saved")
    parser.add_argument("--table", nargs="+", metavar="FILE", help="print saved results as Markdown table rows")
    parser.add_argument("--all", action="store_true", help="with --table: also print counters that are zero")
    arguments = parser.parse_args()
    if arguments.table:
        table(arguments)
    elif arguments.out:
        run(arguments)
    else:
        parser.error("pass --out to run the benchmarks or --table to print saved results")


if __name__ == "__main__":
    main()
