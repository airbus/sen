# === check_depfile.py =================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks the contents of the depfile the generator writes.

The build graph tests read `build.ninja`, so they cover what CMake passes and leave what the
generator then writes uncovered. ninja reads the first target and refuses the file unless it
matches the output the edge attached it to, so there has to be one target, unchanged. A resolved
target, or a second one beside it, costs every import the file carries.

Usage: check_depfile.py <cli_gen> <stl file> <import root>
"""

import subprocess
import sys
import tempfile
from pathlib import Path

# Neither a real path nor an absolute one, so any resolution of it shows in the comparison below.
TARGET = "generated/leaf.stl.h"


def main() -> int:
    """Generates from one model and reads back the depfile. Non-zero on anything wrong with it."""
    generator, stl_file, import_root = sys.argv[1], sys.argv[2], sys.argv[3]

    with tempfile.TemporaryDirectory() as work:
        depfile = Path(work) / "written.d"
        result = subprocess.run(
            [
                generator,
                "cpp",
                "stl",
                "--depfile",
                str(depfile),
                "--depfile-target",
                TARGET,
                "-i",
                import_root,
                stl_file,
            ],
            cwd=work,
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            print(f"the generator failed: {result.stderr}", file=sys.stderr)
            return 1
        if not depfile.is_file():
            print(f"no depfile at {depfile}", file=sys.stderr)
            return 1

        content = depfile.read_text(encoding="utf-8")

    # Counting colons would read a Windows drive letter as a second rule, so the shape is
    # checked by lines: a depfile here is one rule, and ninja reads only the first anyway.
    if content.strip().count("\n") != 0:
        print(f"expected a single rule, got: {content!r}", file=sys.stderr)
        return 1
    if not content.startswith(TARGET + ":"):
        print(f"the target was rewritten or joined: asked for {TARGET!r}, got {content[:120]!r}", file=sys.stderr)
        return 1
    if "imported_base.stl" not in content:
        print(f"the import is missing from the dependencies: {content!r}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
