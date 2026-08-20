# === check_dependency_alignment.py ====================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that dependencies resolve at the profiles' Release default.

The cache design shares one dependency slice per compiler across build
types, which only holds while build_type applies to the sen package alone.
This resolves a Debug consumer graph and fails if any dependency followed.
"""

import json
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
PROFILE = ".conan/profiles/sen_gcc_x86"
CONSUMER_BUILD_TYPE = "Debug"


def misaligned_nodes(graph: dict) -> list[str]:
    """Returns a description of every node that left its expected build_type."""
    problems = []
    for node in graph["graph"]["nodes"].values():
        name = node.get("name") or "conanfile"
        build_type = (node.get("settings") or {}).get("build_type")
        if name == "sen":
            if build_type != CONSUMER_BUILD_TYPE:
                problems.append(f"sen resolved as {build_type}, expected {CONSUMER_BUILD_TYPE}")
        elif build_type not in (None, "Release"):
            problems.append(f"{name} resolved as {build_type}, expected Release")

    return problems


def main() -> int:
    """Resolves the graph and reports any node off its expected build_type."""
    result = subprocess.run(
        [
            "conan",
            "graph",
            "info",
            ".",
            "-pr:a",
            PROFILE,
            "-s",
            f"&:build_type={CONSUMER_BUILD_TYPE}",
            "--format=json",
        ],
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
        text=True,
    )

    problems = misaligned_nodes(json.loads(result.stdout))
    for problem in problems:
        print(problem, file=sys.stderr)

    if problems:
        print("ERROR: build_type must apply to the sen package only.", file=sys.stderr)
        return 1

    print("dependency build_type alignment holds")
    return 0


if __name__ == "__main__":
    sys.exit(main())
