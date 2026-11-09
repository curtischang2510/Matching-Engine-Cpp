#!/usr/bin/env python3
"""Compare the V4 deque with V5 versions on late cancellations."""

import argparse
import random
import shutil
import statistics
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build-instruments"
VERSIONS = ("4.0", "5.0", "5.1", "5.2", "5.4")
EXPECTED = {"messages": 15_000_000, "adds": 14_251_500, "cancels": 748_500}


def fields(output: str) -> dict[str, str]:
    return dict(part.split("=", 1) for part in output.strip().split())


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runs", type=int, default=5)
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")

    cmake = shutil.which("cmake") or str(
        Path.home() / "Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake"
    )
    subprocess.run(
        [cmake, "-S", str(ROOT), "-B", str(BUILD),
         "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
         "-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O2 -g -DNDEBUG -fno-omit-frame-pointer"],
        check=True,
    )
    subprocess.run(
        [cmake, "--build", str(BUILD), "--target", "matching_engine_v4_0",
         "matching_engine_v5_0", "matching_engine_v5_1", "matching_engine_v5_2",
         "matching_engine_v5_4",
         "delayed_cancel_bench", "-j", "4"],
        check=True,
    )

    direct_program = BUILD / "V5/delayed_cancel_bench"
    results: dict[tuple[str, str], list[float]] = {
        (mode, version): [] for mode in ("pipeline", "direct") for version in VERSIONS
    }
    rng = random.Random(42)
    for run in range(1, args.runs + 1):
        cases = list(results)
        rng.shuffle(cases)
        for mode, version in cases:
            if mode == "pipeline":
                family = version.split(".", 1)[0]
                command = [str(ROOT / f"V{family}/matching_engine_v{version.replace('.', '_')}"),
                           "--generated", "15000000"]
            else:
                command = [str(direct_program), version]
            output = subprocess.run(command, check=True, capture_output=True, text=True).stdout
            values = fields(output)
            if any(int(values[key]) != expected for key, expected in EXPECTED.items()):
                raise RuntimeError(f"wrong command counts: {output}")
            if mode == "pipeline":
                if int(values["first_cancel_at"]) != 7_504_501:
                    raise RuntimeError(f"cancellation appeared too early: {output}")
                if int(values["best_bid_qty"]) != 0 or int(values["best_ask_qty"]) != 0:
                    raise RuntimeError(f"book did not clear: {output}")
            seconds = float(values["seconds"])
            results[mode, version].append(seconds)
            print(f"run={run} {mode} {version}: {seconds:.6f}s", flush=True)

    for mode in ("pipeline", "direct"):
        baseline = statistics.median(results[mode, "4.0"])
        for version in VERSIONS:
            median = statistics.median(results[mode, version])
            print(f"{mode} {version}: median={median:.6f}s, speedup={baseline / median:.2f}x")


if __name__ == "__main__":
    main()
