# === conan_lock.py ====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Manages conan.lock for every configuration CI builds.

    conan_lock.py update    regenerate conan.lock from the conanfile
    conan_lock.py check     fail if the conanfile resolves outside conan.lock

PROFILES lists one profile per distinct dependency graph: Linux and Windows
resolve differently, build_type does not. sen_build_docs is in the list
because it adds doxygen as a tool requirement, and conan refuses any
requirement the lockfile does not cover. check resolves with the lockfile
as input, so new upstream revisions do not affect it; it fails only when the
conanfile needs something the lockfile does not cover.
"""

import argparse
import difflib
import subprocess
import sys
import tempfile
from pathlib import Path

PROFILES = ["sen_gcc_x86", "sen_gcc_arm", "sen_clang_x86", "sen_msvc_x86", "sen_build_docs"]

REPO_ROOT = Path(__file__).resolve().parents[2]
LOCKFILE = REPO_ROOT / "conan.lock"


def generate(output: Path, seed: Path | None) -> None:
    """Resolves the graph for every profile, accumulating locked references into output."""
    lock_input = seed
    for profile in PROFILES:
        command = [
            "conan",
            "lock",
            "create",
            ".",
            "-pr:a",
            f".conan/profiles/{profile}",
            f"--lockfile-out={output}",
        ]
        if lock_input is not None:
            command.append(f"--lockfile={lock_input}")
        subprocess.run(command, cwd=REPO_ROOT, check=True)
        lock_input = output


def update() -> int:
    """Regenerates conan.lock from the conanfile."""
    generate(LOCKFILE, seed=None)
    print("conan.lock regenerated")
    return 0


def check() -> int:
    """Fails if resolving the conanfile within conan.lock changes the lockfile."""
    with tempfile.TemporaryDirectory() as temp_dir:
        candidate = Path(temp_dir) / "conan.lock"
        generate(candidate, seed=LOCKFILE)
        current = LOCKFILE.read_text(encoding="utf-8").splitlines(keepends=True)
        resolved = candidate.read_text(encoding="utf-8").splitlines(keepends=True)

    if current != resolved:
        sys.stdout.writelines(difflib.unified_diff(current, resolved, "conan.lock", "resolved"))
        print("ERROR: conan.lock does not cover the current conanfile.", file=sys.stderr)
        print("Run 'python .github/scripts/conan_lock.py update' and commit the result.", file=sys.stderr)
        return 1

    print("conan.lock is up to date")
    return 0


def main() -> int:
    """Runs the requested lockfile operation."""
    parser = argparse.ArgumentParser(
        prog="conan_lock",
        description="Manages conan.lock across every CI dependency-graph shape.",
    )
    parser.add_argument("operation", choices=["update", "check"])
    args = parser.parse_args()

    return update() if args.operation == "update" else check()


if __name__ == "__main__":
    sys.exit(main())
