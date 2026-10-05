# === tap_to_junit.py ==================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Turns a TAP stream into JUnit, for a runner whose own JUnit cannot be trusted.

bats 1.2.1, which the build image carries, writes its junit report with printf and escapes
nothing, so a test whose name contains <, >, & or a quote produces a file no parser will
read. Two installer tests do. Its TAP output has no such problem: a name there is free text
to the end of the line, and the escaping is done here instead.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from xml.etree import ElementTree

# "ok 1 name", "not ok 2 name", either optionally followed by a directive or bats' timing.
RESULT = re.compile(r"^(not )?ok\s+(\d+)\s*(.*)$")
SKIP = re.compile(r"#\s*skip\b(.*)$", re.I)
TIMING = re.compile(r"\s+in\s+\d+(?:\.\d+)?\s*(?:sec|ms)\b\s*$", re.I)


def parse(stream: list[str]) -> list[tuple[str, str, str]]:
    """Returns a (name, status, detail) triple per test, in the order TAP reported them.

    A diagnostic belongs to the test above it: TAP puts the reason for a failure in the
    comment lines that follow the result, so they are collected until the next result.
    """
    tests: list[tuple[str, str, str]] = []
    detail: list[str] = []

    def flush() -> None:
        if tests and detail:
            name, status, _ = tests[-1]
            tests[-1] = (name, status, "\n".join(detail).strip())
        detail.clear()

    for raw in stream:
        line = raw.rstrip("\n")
        match = RESULT.match(line.strip())
        if match is None:
            # The plan line says how many tests to expect and is not one of them.
            if line.startswith("#") and tests:
                detail.append(line.lstrip("# ").rstrip())
            continue

        flush()
        failed, _, rest = match.groups()
        skip = SKIP.search(rest)
        name = TIMING.sub("", SKIP.sub("", rest)).strip()
        status = "failed" if failed else ("skipped" if skip else "passed")
        tests.append((name, status, ""))

    flush()
    return tests


def document(suite: str, tests: list[tuple[str, str, str]]) -> ElementTree.ElementTree:
    """Builds the JUnit tree. ElementTree does the escaping that bats does not."""
    failures = sum(1 for _, status, _ in tests if status == "failed")
    skipped = sum(1 for _, status, _ in tests if status == "skipped")
    root = ElementTree.Element(
        "testsuite",
        name=suite,
        tests=str(len(tests)),
        failures=str(failures),
        errors="0",
        skipped=str(skipped),
    )
    for name, status, detail in tests:
        case = ElementTree.SubElement(root, "testcase", classname=suite, name=name, time="0")
        if status == "failed":
            failure = ElementTree.SubElement(case, "failure", type="failure")
            failure.text = detail
        elif status == "skipped":
            ElementTree.SubElement(case, "skipped")
    return ElementTree.ElementTree(root)


def main() -> int:
    """Reads TAP on standard input and writes the JUnit report named on the command line."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", required=True, help="the classname every case is reported under")
    parser.add_argument("--output", type=Path, required=True, help="where to write the report")
    args = parser.parse_args()

    tests = parse(sys.stdin.readlines())
    if not tests:
        print(f"no TAP results for {args.suite}; no report written", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    document(args.suite, tests).write(args.output, encoding="utf-8", xml_declaration=True)
    print(f"{args.suite}: {len(tests)} cases -> {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
