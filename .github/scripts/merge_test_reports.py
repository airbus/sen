# === merge_test_reports.py ============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Folds per-case JUnit from other runners into the ctest report.

ctest sees one test per runner invocation: the whole vitest suite of a package is a single
pass or fail. The runners themselves can write JUnit per case, so this replaces each such
entry with the cases behind it and the report then has a row for every individual test.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from xml.etree import ElementTree


def case_name(case: ElementTree.Element) -> str:
    """The name ctest or the runner gave the case."""
    return case.get("name") or ""


def failed(case: ElementTree.Element) -> bool:
    """Whether the case carries a failure or an error."""
    return case.find("failure") is not None or case.find("error") is not None


def per_entry_files(extra_dir: Path) -> dict[str, list[Path]]:
    """Groups the per-case reports by the ctest entry each one belongs to.

    A runner that writes a single file is told to name it after its entry, which vitest is.
    One that names its own files cannot be, so it writes into a directory named for the entry
    and everything in there belongs to it. bats is the second kind: it writes one report per
    .bats file, named after that file.
    """
    found: dict[str, list[Path]] = {}
    for path in sorted(extra_dir.glob("*.xml")):
        found.setdefault(path.stem, []).append(path)
    for directory in sorted(path for path in extra_dir.iterdir() if path.is_dir()):
        files = sorted(directory.glob("*.xml"))
        if files:
            found.setdefault(directory.name, []).extend(files)
    return found


def restate(root: ElementTree.Element) -> None:
    """Rewrites the totals a suite element carries, which the fold has just made wrong.

    ctest wrote them for the entries it ran. Expanding one of those into the cases behind it
    changes every count, and a reader that trusts the attributes rather than counting the rows
    would be told there are fewer tests than the file lists.

    The root counts too. Merging the main and flaky passes wraps the suites in a `testsuites`,
    so a guard that accepts only `testsuite` skips the one element every reader looks at first.
    """
    for suite in [root, *root.iter("testsuite")]:
        if suite.tag not in ("testsuite", "testsuites"):
            continue
        cases = list(suite.iter("testcase"))
        suite.set("tests", str(len(cases)))
        suite.set("failures", str(sum(1 for case in cases if failed(case))))
        suite.set("skipped", str(sum(1 for case in cases if case.find("skipped") is not None)))


def merge(report: Path, extra_dir: Path) -> tuple[ElementTree.ElementTree[ElementTree.Element[str]], list[str]]:
    """Returns the report with each runner entry expanded, and a line per expansion."""
    tree = ElementTree.parse(report)
    root = tree.getroot()

    # The parent of a testcase is needed to remove it, and ElementTree has no parent link.
    parents = {child: parent for parent in root.iter() for child in parent}
    by_name: dict[str, ElementTree.Element] = {}
    for case in root.iter("testcase"):
        by_name.setdefault(case_name(case), case)

    notes: list[str] = []
    for entry, paths in sorted(per_entry_files(extra_dir).items()):
        wrapper = by_name.get(entry)
        if wrapper is None:
            # The file is from an earlier run: this one did not run that test. Counting it
            # would report tests that were never executed.
            notes.append(f"{entry}: no such ctest entry in this run, not merged")
            continue

        cases = [case for path in paths for case in ElementTree.parse(path).getroot().iter("testcase")]
        if not cases:
            where = ", ".join(path.name for path in paths)
            notes.append(f"{entry}: no cases in {where}, left as one entry")
            continue

        suite = parents[wrapper]
        position = list(suite).index(wrapper)
        suite.remove(wrapper)

        # The spec path alone repeats across packages, so the ctest entry stays in front of
        # it. That keeps every case traceable to the command that ran it.
        for offset, case in enumerate(cases):
            case.set("classname", f"{entry}/{case.get('classname', '')}".rstrip("/"))
            suite.insert(position + offset, case)

        # A runner can fail for a reason no case reports, such as a non-zero exit after the
        # last test. Dropping its entry would drop that failure, so it is kept.
        if failed(wrapper) and not any(failed(case) for case in cases):
            wrapper.set("name", f"{entry} (runner)")
            wrapper.set("classname", entry)
            suite.insert(position + len(cases), wrapper)
            notes.append(f"{entry}: {len(cases)} cases, all passed, but the runner failed")
        else:
            plural = "" if len(cases) == 1 else "s"
            notes.append(f"{entry}: expanded into {len(cases)} case{plural}")

    restate(root)
    return tree, notes


def main() -> int:
    """Writes the folded report named on the command line."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, required=True, help="the ctest JUnit report")
    parser.add_argument("--extra-dir", type=Path, required=True, help="directory of per-case JUnit")
    parser.add_argument("--output", type=Path, required=True, help="where to write the merged report")
    args = parser.parse_args()

    if not args.report.is_file():
        print(f"no report at {args.report}", file=sys.stderr)
        return 1

    if not args.extra_dir.is_dir():
        print(f"no per-case reports at {args.extra_dir}; copying the report unchanged")
        args.output.write_bytes(args.report.read_bytes())
        return 0

    tree, notes = merge(args.report, args.extra_dir)
    for note in notes:
        print(note)

    tree.write(args.output, encoding="utf-8", xml_declaration=True)
    total = sum(1 for _ in ElementTree.parse(args.output).getroot().iter("testcase"))
    print(f"wrote {args.output} with {total} cases")
    return 0


if __name__ == "__main__":
    sys.exit(main())
