#!/usr/bin/env python3
# === analysable_sources.py ============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Filters a list of paths down to the ones a compile database has a command for.

clang-tidy can only analyse a translation unit it knows how to compile. Given a header, it falls
back to a bare invocation with no include paths and no defines, and every check then reports
nonsense: misc-include-cleaner in particular reports every symbol as unprovided, std::vector
included. So the changed-lines lane asks this which of the changed files it may pass on.

Reads paths on standard input, one per line, and writes the analysable ones to standard output.
"""

import argparse
import json
import os
import sys


def analysable(compile_commands: str, paths: list[str]) -> list[str]:
    """Returns the paths that appear as a translation unit in the compile database."""
    with open(compile_commands, encoding="utf-8") as handle:
        entries = json.load(handle)

    # A database entry's "file" may be relative to its "directory", and either side may reach the
    # same file by a different route, so both are resolved before comparing.
    known = set()
    for entry in entries:
        path = entry["file"]
        if not os.path.isabs(path):
            path = os.path.join(entry.get("directory", ""), path)
        known.add(os.path.realpath(path))

    return [p for p in paths if os.path.realpath(p) in known]


def main() -> int:
    """Filters standard input against the given compile database."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compile_commands", help="path to compile_commands.json")
    args = parser.parse_args()

    paths = [line.strip() for line in sys.stdin if line.strip()]
    for path in analysable(args.compile_commands, paths):
        print(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
