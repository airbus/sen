#!/usr/bin/env python3
# === run_shuffled_tests.py ============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Runs each unit test binary whole, in a shuffled order, to find tests that depend on other tests.

Nothing else in this build does that. `gtest_discover_tests` gives every test its own ctest entry and
its own process, so a test that only passes because another one ran first cannot fail any lane, and
nothing reports it either. `--schedule-random` shuffles ctest's order, which is one process per test,
so it does not exercise this at all.

Run it from a configured build directory:

    python3 tools/run_shuffled_tests.py --build-dir build/gcc/Release

The binaries come from ctest's own description of the tests, so nothing here has to be kept in step
with the build. Exit status is 0 when every order passed.
"""

from __future__ import annotations

import argparse
import collections
import json
import re
import subprocess
import sys
from pathlib import Path

FAILED = re.compile(r"^\[  FAILED  \] ([A-Za-z_][\w./<>, ]*)$", re.M)


def gtest_binaries(build_dir: Path) -> list[str]:
    """The distinct executables behind the tests ctest invokes with --gtest_filter."""
    listing = subprocess.run(
        ["ctest", "--show-only=json-v1"],
        cwd=build_dir,
        capture_output=True,
        text=True,
        check=False,
    )
    if listing.returncode != 0:
        print(f"ctest could not list the tests in {build_dir}:\n{listing.stderr}", file=sys.stderr)
        return []

    tests = json.loads(listing.stdout).get("tests", [])
    counts: collections.Counter[str] = collections.Counter()
    for test in tests:
        command = test.get("command") or []
        if any("--gtest_filter=" in str(argument) for argument in command):
            counts[command[0]] += 1
    return [exe for exe, _ in counts.most_common()]


def run_one(exe: str, seeds: int) -> list[tuple[int, list[str]]]:
    """Every seed whose order failed, with the tests that failed in it."""
    bad: list[tuple[int, list[str]]] = []
    for seed in range(1, seeds + 1):
        result = subprocess.run(
            [exe, "--gtest_shuffle", f"--gtest_random_seed={seed}"],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            names = sorted(set(FAILED.findall(result.stdout)))
            bad.append((seed, names))
    return bad


def main() -> int:
    """Runs every selected binary in several orders and reports what the order changed."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", default=".", type=Path, help="a configured build directory")
    parser.add_argument("--seeds", default=5, type=int, help="orders to try per binary")
    parser.add_argument("--only", default="", help="substring: run just the binaries whose name contains it")
    args = parser.parse_args()

    binaries = [exe for exe in gtest_binaries(args.build_dir) if args.only in Path(exe).name]
    if not binaries:
        print("no gtest binaries found; is the build configured and built?", file=sys.stderr)
        return 1

    print(f"{len(binaries)} binaries, {args.seeds} orders each")
    order_dependent = 0
    for exe in binaries:
        name = Path(exe).name
        bad = run_one(exe, args.seeds)
        if not bad:
            print(f"  ok      {name}")
            continue

        # A test that fails in every order is failing on its own account: a broken test, or a build
        # whose environment it cannot satisfy. Only a test that fails in some orders and not others is
        # reporting the order, and that is the whole point of this run.
        always = set.intersection(*(set(names) for _, names in bad)) if len(bad) == args.seeds else set()
        sometimes = sorted({n for _, names in bad for n in names} - always)

        if sometimes or any(not names for _, names in bad):
            order_dependent += 1
            print(f"  ORDER   {name}: failed in {len(bad)} of {args.seeds} orders")
            for seed, names in bad:
                listed = ", ".join(sorted(set(names) - always)) or "no test named; the binary died"
                print(f"            seed {seed}: {listed}")
        else:
            print(f"  same    {name}: {len(always)} test(s) fail in every order, so not order-dependent")
        for name_always in sorted(always):
            print(f"            always: {name_always}")

    if order_dependent:
        print(f"\n{order_dependent} binary(ies) have tests that depend on the order of other tests.")
        print("Reproduce one with: <binary> --gtest_shuffle --gtest_random_seed=<seed>")
        return 1
    print("\nno test depends on another")
    return 0


if __name__ == "__main__":
    sys.exit(main())
