# === summarise_sanitizer_reports.py ===================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Groups the sanitizer reports of a run into something a person can act on.

A sanitizer prints one block per finding, and a run repeats the same defect
across every test that reaches it. Raw, that reads as a crisis; grouped by the
sen:: frames that produced it, it reads as the handful of families it actually is.

Two shapes are parsed. Thread, address and leak sanitizers write
"WARNING/ERROR: <X>Sanitizer: <kind>"; undefined behaviour writes
"<file>:<line>:<col>: runtime error:" and neither its own name nor a stack.

A race is a pair, so the family is the first sen:: frame on each side: one anchor
plus whatever the other side happened to be collapses distinct families together.
Undefined behaviour has no frames to pair, so its location is its identity.

This reports rather than judges by default, because the nightly lanes exist to collect
an inventory and one abort would hide what follows it. --fail-on-findings makes it judge,
for the lane that gates: a finding reaches the report files even when it happened in a
process ctest never waited on, or in a test whose verdict was inverted, so the files are
the one place every finding can be seen at once.
"""

import argparse
import os
import re
import sys
from collections import Counter
from pathlib import Path

# "WARNING: ThreadSanitizer: data race on vptr (ctor/dtor vs virtual call) (pid=1)"
# "ERROR: AddressSanitizer: heap-use-after-free on address 0x60300000eff0"
FINDING = re.compile(r"(?:WARNING|ERROR): (\w+Sanitizer): (.+)")

# "libs/core/include/sen/core/base/checked_conversions.h:92:34: runtime error: nan is ..."
# UndefinedBehaviorSanitizer says neither WARNING nor its own name on this line. The path is
# matched loosely because a build directory may contain a space, and \S+? cannot cross one.
UBSAN = re.compile(r"^(?P<where>.+?):(?P<line>\d+):(?P<col>\d+): runtime error: (?P<kind>.+?)\s*$", re.M)

# A runtime that could not do its job writes this into the log where a finding would go, and
# with log_path set it writes nothing to stderr -- so every test goes red and the summary
# beside them used to say there were no findings.
MALFUNCTION = re.compile(r"(\w+Sanitizer): (failed to (?:read|parse) suppressions[^\n]*|CHECK failed:[^\n]*)")

# "on vptr" is part of the kind; "on address" is not, nor is a pid or an aside.
KIND_TAIL = re.compile(r"\s+on address\b|\s+on 0x|\s+\(")

# "    #3 sen::impl::WorkQueueImpl::clear() /workspace/libs/core/src/obj/work_queue.cpp:132:5"
SEN_FRAME = re.compile(r"#\d+ (sen::[A-Za-z0-9_:~]+)")
FRAME = re.compile(r"^\s*#\d+ ")

# A block is divided into sides by its own headers: "Previous write of size 4 at
# 0x... by thread T1:", "Mutex M0 previously acquired by the same thread here:".
# Anything ending in a colon that is not a frame opens a new side.
SIDE = re.compile(r":\s*$")

# The repository-relative half of a frame's path, which is what a reader can open.
SOURCE = re.compile(r"((?:libs|apps|components|tools)/[A-Za-z0-9_/.]+\.(?:cpp|h)):(\d+)")

# The same tail without a line number, for paths the sanitizer prints on their own.
REPO_PATH = re.compile(r"((?:libs|apps|components|tools)/[A-Za-z0-9_/.]+)$")


def anchors(block: str) -> list[str]:
    """First sen:: frame of each side of a finding, in the order they appear.

    A race names two sides and the pair identifies the family. Sides with no
    sen:: frame contribute nothing: a finding entirely inside third-party code
    is anchored by whichever side does reach us.
    """
    found: list[str] = []
    current: str | None = None

    for line in block.splitlines():
        if not FRAME.match(line) and SIDE.search(line):
            if current:
                found.append(current)
            current = None
            continue
        match = SEN_FRAME.search(line)
        if match and current is None:
            current = match.group(1)

    if current:
        found.append(current)
    return found


def relative(path: str) -> str:
    """Trims an absolute build path down to the repository-relative part."""
    match = REPO_PATH.search(path)
    return match.group(1) if match else path


def undefined_behaviour(text: str) -> list[dict]:
    """One entry per UBSan diagnostic.

    These carry no stack, so the source location is both the site and the only
    thing later runs can be matched on.
    """
    entries = []
    for match in UBSAN.finditer(text):
        site = f"{relative(match.group('where'))}:{match.group('line')}:{match.group('col')}"
        entries.append(
            {
                "tool": "UndefinedBehaviorSanitizer",
                "kind": match.group("kind"),
                "sites": [site],
                "site": site,
                "sources": [f"{relative(match.group('where'))}:{match.group('line')}"],
            }
        )
    return entries


def findings(text: str) -> list[dict]:
    """Splits sanitizer output into one entry per finding.

    A block runs from its header line to the next one, so the frames in between
    are the ones that produced it.
    """
    starts = [m.start() for m in FINDING.finditer(text)]
    entries = []
    for index, start in enumerate(starts):
        end = starts[index + 1] if index + 1 < len(starts) else len(text)
        block = text[start:end]
        match = FINDING.search(block)
        if match is None:
            continue

        sites = anchors(block)
        entries.append(
            {
                "tool": match.group(1),
                "kind": KIND_TAIL.split(match.group(2))[0].strip(),
                "sites": sites,
                "site": sites[0] if sites else "(no sen:: frame)",
                "sources": [f"{path}:{line}" for path, line in SOURCE.findall(block)],
            }
        )

    return entries + undefined_behaviour(text)


def malfunctions(text: str) -> list[str]:
    """Sanitizer failures that are not findings and must not read as their absence."""
    return sorted({f"{match.group(1)}: {match.group(2).strip()}" for match in MALFUNCTION.finditer(text)})


def nothing_found(reports: int | None) -> str:
    """What to say when a run produced no findings, without saying more than is known.

    Saying "no findings" over nothing read is how a lane that saw nothing and a lane that
    looked at nothing came to print the same sentence. Whether the lane can detect at all
    is check_sanitizer_lane.py's question, not this one.
    """
    if reports == 0:
        return "No sanitizer reports were produced in this run.\n"
    if reports is not None:
        return f"No sanitizer findings in {reports} report file(s).\n"
    return "No sanitizer findings in this run.\n"


def malfunction_lines(broken: list[str], any_findings: bool) -> list[str]:
    """The opening of a summary whose run the sanitizer could not properly make."""
    opening = (
        "The sanitizer could not do its job in this run, so what follows is not a complete account."
        if any_findings
        else "The sanitizer could not do its job in this run, so its silence says nothing."
    )
    return [opening, *(f"- `{problem}`" for problem in broken), *([""] if any_findings else [])]


def summarise(entries: list[dict], reports: int | None = None, broken: list[str] | None = None) -> str:
    """Renders the grouped findings as markdown.

    Grouped by family -- tool, kind and the pair of sen:: frames -- because that
    is the unit a later run can be matched against. Line numbers move; symbols
    survive a refactor.
    """
    opening = malfunction_lines(broken, bool(entries)) if broken else []
    if broken and not entries:
        return "\n".join(opening) + "\n"

    if not entries:
        return nothing_found(reports)

    def family(entry: dict) -> tuple:
        return (entry["tool"], entry["kind"], tuple(entry["sites"]))

    by_family = Counter(family(entry) for entry in entries)
    sources: dict[tuple, Counter] = {}
    for entry in entries:
        sources.setdefault(family(entry), Counter()).update(entry["sources"])

    def count_of(number: int, one: str, many: str) -> str:
        return f"{number} {one if number == 1 else many}"

    lines = [
        *opening,
        f"{count_of(len(entries), 'sanitizer finding', 'sanitizer findings')}, "
        f"{count_of(len(by_family), 'distinct family', 'distinct families')}.",
        "",
    ]
    lines.append("| count | tool | kind | first sen:: frame | other side |")
    lines.append("|---|---|---|---|---|")
    for (tool, kind, sites), count in by_family.most_common():
        first = f"`{sites[0]}`" if sites else "_(no sen:: frame)_"
        other = f"`{sites[1]}`" if len(sites) > 1 else "_(one side only)_"
        lines.append(f"| {count} | {tool} | {kind} | {first} | {other} |")

    lines.append("")
    for key, count in by_family.most_common():
        common = ", ".join(f"`{where}`" for where, _ in sources[key].most_common(4))
        if not common:
            continue
        _, kind, sites = key
        lines.append(f"- `{sites[0] if sites else kind}` ({count}): {common}")

    return "\n".join(lines) + "\n"


def main() -> int:
    """Writes the grouped summary to stdout and to GITHUB_STEP_SUMMARY."""
    parser = argparse.ArgumentParser(
        prog="summarise_sanitizer_reports",
        description="Groups a run's sanitizer findings by the site that produced them.",
    )
    parser.add_argument("paths", nargs="+", help="Report files or directories holding them.")
    parser.add_argument(
        "--fail-on-findings",
        action="store_true",
        help="Exit non-zero when the run produced findings, for a lane that gates rather than collects.",
    )
    args = parser.parse_args()

    text = []
    unreadable = []
    for raw in args.paths:
        path = Path(raw)
        if path.is_dir():
            # os.walk reports what it could not open; rglob swallows it, and a directory
            # nobody can read then reads as a directory with nothing in it.
            # Consumed, not just created: os.walk is lazy and reports nothing until it runs.
            list(os.walk(path, onerror=lambda error: unreadable.append(f"{error.filename}: {error.strerror}")))
            children = sorted(path.rglob("*"))
        else:
            children = [path] if path.is_file() else []
        for child in children:
            if not child.is_file():
                continue
            try:
                text.append(child.read_text(encoding="utf-8", errors="replace"))
            except OSError as error:
                # A container running as root leaves its reports unreadable by the job's
                # user. Unread is not the same as absent, and used to render as absent.
                unreadable.append(f"{child}: {error.strerror}")

    joined = "\n".join(text)
    entries = findings(joined)
    broken = malfunctions(joined) + [f"could not be read -- {problem}" for problem in unreadable]
    report = summarise(entries, reports=len(text), broken=broken)
    print(report, end="")

    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as handle:
            handle.write(report)

    # Silence is not judged here: a lane that produced nothing may have looked at nothing,
    # and check_sanitizer_lane.py is what settles that before the suite runs.
    return 1 if (args.fail_on_findings and (entries or broken)) else 0


if __name__ == "__main__":
    sys.exit(main())
