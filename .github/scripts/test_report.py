# === test_report.py ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Renders the ctest JUnit report as a job summary.

A report with no tests in it is an error: a build configured without tests
runs the suite, finds nothing and would otherwise pass.
"""

import sys
from pathlib import Path
from xml.etree import ElementTree


class EmptyReport(Exception):
    """Raised when the report holds no test cases."""


def read_report(path: Path) -> tuple[int, int, int, list[str]]:
    """Returns totals and the names of the failed cases."""
    cases = ElementTree.parse(path).getroot().iter("testcase")

    total = 0
    failed: list[str] = []
    skipped = 0
    for case in cases:
        total += 1
        if case.find("skipped") is not None:
            skipped += 1
        elif case.find("failure") is not None or case.find("error") is not None:
            failed.append(f"{case.get('classname', '')}.{case.get('name', '?')}".lstrip("."))

    if total == 0:
        raise EmptyReport(f"{path} holds no test cases")

    return total, skipped, total - skipped - len(failed), failed


def render(total: int, skipped: int, passed: int, failed: list[str]) -> str:
    """Returns the markdown for the job summary."""
    lines = [
        "### Tests",
        "",
        "| Total | Passed | Failed | Skipped |",
        "| ----: | -----: | -----: | ------: |",
        f"| {total} | {passed} | {len(failed)} | {skipped} |",
    ]
    if failed:
        lines += ["", "Failed:", ""] + [f"- `{name}`" for name in failed]
    return "\n".join(lines) + "\n"


def main() -> int:
    """Prints the summary for the report named on the command line."""
    if len(sys.argv) != 2:
        print("usage: test_report.py <ctestReport.xml>", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    if not path.is_file():
        print(f"no test report at {path}", file=sys.stderr)
        return 1

    try:
        total, skipped, passed, failed = read_report(path)
    except EmptyReport as empty:
        print(empty, file=sys.stderr)
        return 1

    print(render(total, skipped, passed, failed), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
