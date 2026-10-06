#!/usr/bin/env python3
"""Measures what building and testing Carbon costs, for Docs/Optimizations.md.

    python Scripts/BuildMetrics.py --dawn-debug <prefix> --dawn-release <prefix> --out Scratch/BuildMetrics.json

Run it where the compiler is set up (on Windows: a Visual Studio developer prompt) and leave the machine alone
meanwhile. Per configuration (Debug, Release), with Ninja, each `--runs` times, reporting the median:

  - a clean build of all targets in an empty build directory: wall-clock seconds, and the seconds of all build
    steps added up from .ninja_log ("CPU seconds": what the build costs on one core);
  - the rebuild after touching Framework/src/Carbon/Core/Log.h, and after changing the project version in the root
    CMakeLists.txt (which is restored afterwards);
  - Release only: `ctest` for the whole suite and for the renderer backends' tests.

It also reads the duration of every job of the latest successful CI run on main from GitHub's public API.
`--only` limits what is measured, `--ctest-args` adds arguments to ctest (for example "--parallel 8").
"""

import argparse
import datetime
import json
import os
import pathlib
import platform
import re
import shlex
import shutil
import statistics
import subprocess
import sys
import time
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parent.parent
REPOSITORY = "eliasertl/Carbon"
STEPS = ("clean", "incremental", "ctest", "ci")


def run(command, **options):
    """Runs a command quietly and returns the seconds it took. Its output is shown only when it fails."""
    start = time.perf_counter()
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **options)
    seconds = time.perf_counter() - start
    if result.returncode != 0:
        sys.stderr.write(result.stdout[-6000:])
        sys.exit(f"Failed ({result.returncode}): {' '.join(map(str, command))}")
    return seconds


def read_ninja_seconds(build, since=0):
    """Adds up the durations of the build steps in .ninja_log, from line `since` on. Returns (seconds, lines)."""
    lines = (build / ".ninja_log").read_text(encoding="utf-8", errors="replace").splitlines()
    total = 0
    for line in lines[since:]:
        fields = line.split("\t")
        if len(fields) >= 4 and fields[0].isdigit():
            total += int(fields[1]) - int(fields[0])
    return total / 1000.0, len(lines)


def remove_tree(path):
    # Windows refuses to delete read-only files, which git submodule checkouts in a build tree can leave behind.
    def make_writable(function, target, _):
        os.chmod(target, 0o700)
        function(target)

    if path.exists():
        shutil.rmtree(path, onerror=make_writable)


def configure(arguments, source, build, build_type):
    prefix = arguments.dawn_debug if build_type == "Debug" else arguments.dawn_release
    command = ["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={build_type}",
               "-DCARBON_WARNINGS_AS_ERRORS=ON"]
    if prefix:
        command.append(f"-DCMAKE_PREFIX_PATH={pathlib.Path(prefix).as_posix()}")
    command += shlex.split(arguments.cmake_args)
    return run(command)


def measure_configuration(arguments, source, work, build_type, steps):
    build = work / build_type
    result = {}
    if "clean" in steps:
        result["configure_s"], result["clean_build_s"], result["clean_build_cpu_s"] = [], [], []
        for index in range(arguments.runs):
            remove_tree(build)
            result["configure_s"].append(configure(arguments, source, build, build_type))
            result["clean_build_s"].append(run(["cmake", "--build", str(build)]))
            result["clean_build_cpu_s"].append(read_ninja_seconds(build)[0])
            print(f"  {build_type} clean build {index + 1}: {result['clean_build_s'][-1]:.1f} s "
                  f"({result['clean_build_cpu_s'][-1]:.0f} CPU s)", flush=True)
    elif not (build / "build.ninja").exists():
        configure(arguments, source, build, build_type)
        run(["cmake", "--build", str(build)])

    if "incremental" in steps:
        run(["cmake", "--build", str(build)])  # up to date before anything is touched
        log_header = source / "Framework" / "src" / "Carbon" / "Core" / "Log.h"
        result["touch_log_h_s"], result["touch_log_h_cpu_s"] = [], []
        for index in range(arguments.runs):
            time.sleep(1.1)  # file times have a resolution of up to a second
            os.utime(log_header)
            lines = read_ninja_seconds(build)[1]
            result["touch_log_h_s"].append(run(["cmake", "--build", str(build)]))
            result["touch_log_h_cpu_s"].append(read_ninja_seconds(build, lines)[0])
            print(f"  {build_type} rebuild after touching Log.h {index + 1}: {result['touch_log_h_s'][-1]:.1f} s",
                  flush=True)

        lists = source / "CMakeLists.txt"
        original = lists.read_text(encoding="utf-8")
        pattern = re.compile(r"(project\(Carbon VERSION \d+\.\d+\.)(\d+)")
        result["version_change_s"], result["version_change_cpu_s"] = [], []
        try:
            for index in range(arguments.runs):
                time.sleep(1.1)
                patch = int(pattern.search(original).group(2)) + index + 1
                lists.write_text(pattern.sub(lambda match: match.group(1) + str(patch), original), encoding="utf-8",
                                 newline="\n")
                lines = read_ninja_seconds(build)[1]
                result["version_change_s"].append(run(["cmake", "--build", str(build)]))
                result["version_change_cpu_s"].append(read_ninja_seconds(build, lines)[0])
                print(f"  {build_type} rebuild after a version change {index + 1}: "
                      f"{result['version_change_s'][-1]:.1f} s", flush=True)
        finally:
            lists.write_text(original, encoding="utf-8", newline="\n")
        run(["cmake", "--build", str(build)])

    if "ctest" in steps and build_type == "Release":
        ctest = ["ctest", "--test-dir", str(build)] + shlex.split(arguments.ctest_args)
        result["ctest_s"], result["ctest_backends_s"] = [], []
        for index in range(arguments.runs):
            result["ctest_s"].append(run(ctest))
            result["ctest_backends_s"].append(run(ctest + ["-R", "^Backends/"]))
            print(f"  ctest {index + 1}: {result['ctest_s'][-1]:.1f} s, backends "
                  f"{result['ctest_backends_s'][-1]:.1f} s", flush=True)
        listing = subprocess.run(["ctest", "--test-dir", str(build), "-N"], stdout=subprocess.PIPE, text=True).stdout
        match = re.search(r"Total Tests: (\d+)", listing)
        result["ctest_entries"] = int(match.group(1)) if match else 0
    return result


def fetch_json(url):
    request = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json",
                                                   "User-Agent": "Carbon-BuildMetrics"})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)
    except urllib.error.URLError:
        # A Python without certificates (some bundled ones on Windows): curl uses the system's.
        output = subprocess.run(["curl", "-sSL", "-H", "Accept: application/vnd.github+json", url], check=True,
                                stdout=subprocess.PIPE).stdout
        return json.loads(output)


def parse_time(text):
    return datetime.datetime.fromisoformat(text.replace("Z", "+00:00"))


def measure_ci(run_id=None):
    """Job durations of a CI run: the latest successful one on main, or the run with `run_id`."""
    api = f"https://api.github.com/repos/{REPOSITORY}/actions"
    if run_id:
        ci_run = fetch_json(f"{api}/runs/{run_id}")
    else:
        runs = fetch_json(f"{api}/workflows/CI.yml/runs?branch=main&status=success&per_page=1")["workflow_runs"]
        if not runs:
            return {}
        ci_run = runs[0]
    jobs = fetch_json(f"{api}/runs/{ci_run['id']}/jobs?per_page=100")["jobs"]
    durations = {}
    for job in jobs:
        if job.get("started_at") and job.get("completed_at") and job.get("conclusion") == "success":
            durations[job["name"]] = (parse_time(job["completed_at"]) - parse_time(job["started_at"])).total_seconds()
    wall = (parse_time(ci_run["updated_at"]) - parse_time(ci_run["run_started_at"])).total_seconds()
    return {"run_id": ci_run["id"], "url": ci_run["html_url"], "commit": ci_run["head_sha"],
            "conclusion": ci_run.get("conclusion"), "wall_s": wall, "jobs_s": durations}


def describe_machine(work):
    machine = {"os": platform.platform(), "cpu": platform.processor(), "cores": os.cpu_count()}
    for compiler_file in work.glob("*/CMakeFiles/*/CMakeCXXCompiler.cmake"):
        text = compiler_file.read_text(encoding="utf-8", errors="replace")
        identifier = re.search(r'set\(CMAKE_CXX_COMPILER_ID "([^"]*)"', text)
        version = re.search(r'set\(CMAKE_CXX_COMPILER_VERSION "([^"]*)"', text)
        if identifier and version:
            machine["compiler"] = f"{identifier.group(1)} {version.group(1)}"
            break
    return machine


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", default=str(ROOT), help="the source tree to build (default: this checkout)")
    parser.add_argument("--work", default=str(ROOT / "Build" / "Metrics"), help="where the build directories go")
    parser.add_argument("--dawn-debug", default="", help="install prefix of a Debug Dawn")
    parser.add_argument("--dawn-release", default="", help="install prefix of a Release Dawn")
    parser.add_argument("--cmake-args", default="", help="more arguments for the CMake configuration")
    parser.add_argument("--ctest-args", default="", help="more arguments for ctest, for example '--parallel 8'")
    parser.add_argument("--runs", type=int, default=3, help="how often each measurement is taken")
    parser.add_argument("--only", nargs="+", choices=STEPS, default=list(STEPS), help="what to measure")
    parser.add_argument("--configurations", nargs="+", default=["Debug", "Release"])
    parser.add_argument("--ci-run", default="", help="the id of the CI run to read instead of the latest green one")
    parser.add_argument("--out", required=True, help="where the results are saved (JSON)")
    arguments = parser.parse_args()

    source = pathlib.Path(arguments.source).resolve()
    work = pathlib.Path(arguments.work).resolve()
    results = {"runs": arguments.runs, "configurations": {}}
    commit = subprocess.run(["git", "-C", str(source), "rev-parse", "HEAD"], stdout=subprocess.PIPE, text=True)
    results["commit"] = commit.stdout.strip()

    if set(arguments.only) & {"clean", "incremental", "ctest"}:
        for build_type in arguments.configurations:
            print(f"{build_type}:", flush=True)
            results["configurations"][build_type] = measure_configuration(arguments, source, work, build_type,
                                                                           arguments.only)
        results["machine"] = describe_machine(work)
    if "ci" in arguments.only:
        results["ci"] = measure_ci(arguments.ci_run)

    out = pathlib.Path(arguments.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(results, indent=1), encoding="utf-8")

    print("\nMedians:")
    for build_type, measurements in results["configurations"].items():
        for name, values in measurements.items():
            if isinstance(values, list) and values:
                print(f"  {build_type} {name}: {statistics.median(values):.1f}")
            elif not isinstance(values, list):
                print(f"  {build_type} {name}: {values}")
    for name, seconds in results.get("ci", {}).get("jobs_s", {}).items():
        print(f"  CI {name}: {seconds:.0f} s")
    print(f"Saved to {out}")


if __name__ == "__main__":
    main()
