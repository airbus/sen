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
from typing import NamedTuple
from xml.etree import ElementTree


class EmptyReport(Exception):
    """Raised when the report holds no test cases."""


class Case(NamedTuple):
    """One case from the report: what it is, what happened, how long it took."""

    classname: str
    name: str
    status: str  # passed, failed or skipped
    seconds: float
    detail: str

    @property
    def full_name(self) -> str:
        """The name as ctest identifies it, which is what a reader greps for.

        ctest repeats the whole name in both attributes while gtest's own writer splits them, so
        joining the two blindly doubles a ctest name. Kept in step with split_name in
        render_test_document, which decides the same thing for the document.
        """
        if self.classname and self.classname != self.name:
            return f"{self.classname}.{self.name}"
        return self.name


def read_cases(path: Path) -> list[Case]:
    """Returns every case in the report, in the order ctest wrote them.

    One reader for both outputs: the job summary and the QA document would
    otherwise each decide what counts as a failure, and disagree about the
    same run.
    """
    cases: list[Case] = []
    for case in ElementTree.parse(path).getroot().iter("testcase"):
        failure = case.find("failure")
        if failure is None:
            failure = case.find("error")

        if case.find("skipped") is not None:
            status, detail = "skipped", ""
        elif failure is not None:
            status = "failed"
            detail = (failure.get("message") or failure.text or "").strip()

            # ctest records the reason as the bare word "Failed" and puts the test's output
            # in system-out, so the message alone tells a reader nothing. gtest's own writer
            # does put the assertion in the message, which is why a real one still wins.
            if detail in ("", "Failed"):
                output = case.find("system-out")
                captured = (output.text or "").strip() if output is not None else ""
                if captured:
                    detail = captured
        else:
            status, detail = "passed", ""

        try:
            seconds = float(case.get("time") or 0.0)
        except ValueError:
            seconds = 0.0

        cases.append(Case(case.get("classname", ""), case.get("name", "?"), status, seconds, detail))

    return cases


def read_report(path: Path) -> tuple[int, int, int, list[str]]:
    """Returns totals and the names of the failed cases."""
    cases = read_cases(path)

    if not cases:
        raise EmptyReport(f"{path} holds no test cases")

    skipped = sum(1 for case in cases if case.status == "skipped")
    failed = [case.full_name for case in cases if case.status == "failed"]

    return len(cases), skipped, len(cases) - skipped - len(failed), failed


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
