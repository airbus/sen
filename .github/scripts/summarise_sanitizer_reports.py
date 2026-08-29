# === summarise_sanitizer_reports.py ===================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Groups the sanitizer reports of a run into something a person can act on.

A sanitizer prints one block per finding, and a run repeats the same defect
across every test that reaches it. Raw, that reads as a crisis; grouped by the
first frame inside sen::, it reads as the handful of sites it actually is.

This reports rather than judges: the exit status says whether the summary was
written, not whether findings were found.
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

# "on vptr" is part of the kind; "on address" is not, nor is a pid or an aside.
KIND_TAIL = re.compile(r"\s+on address\b|\s+on 0x|\s+\(")

# "    #3 sen::impl::WorkQueueImpl::clear() /workspace/libs/core/src/obj/work_queue.cpp:132:5"
SEN_FRAME = re.compile(r"#\d+ (sen::[A-Za-z0-9_:~]+)")

# The repository-relative half of a frame's path, which is what a reader can open.
SOURCE = re.compile(r"((?:libs|apps|components|tools)/[A-Za-z0-9_/.]+\.(?:cpp|h)):(\d+)")


def findings(text: str) -> list[dict]:
    """Splits a sanitizer log into one entry per finding.

    A finding runs from its WARNING/ERROR line to the next one, so the frames
    in between are the ones that produced it.
    """
    starts = [m.start() for m in FINDING.finditer(text)]
    entries = []
    for index, start in enumerate(starts):
        end = starts[index + 1] if index + 1 < len(starts) else len(text)
        block = text[start:end]
        match = FINDING.search(block)
        if match is None:
            continue

        frame = SEN_FRAME.search(block)
        entries.append(
            {
                "tool": match.group(1),
                "kind": KIND_TAIL.split(match.group(2))[0].strip(),
                "site": frame.group(1) if frame else "(no sen:: frame)",
                "sources": [f"{path}:{line}" for path, line in SOURCE.findall(block)],
            }
        )
    return entries


def summarise(entries: list[dict]) -> str:
    """Renders the grouped findings as markdown."""
    if not entries:
        return "No sanitizer findings in this run.\n"

    by_site = Counter((entry["tool"], entry["kind"], entry["site"]) for entry in entries)
    sources: dict[tuple, Counter] = {}
    for entry in entries:
        key = (entry["tool"], entry["kind"], entry["site"])
        sources.setdefault(key, Counter()).update(entry["sources"])

    lines = [f"{len(entries)} sanitizer findings, {len(by_site)} distinct sites.", ""]
    lines.append("| count | tool | kind | first sen:: frame |")
    lines.append("|---|---|---|---|")
    for (tool, kind, site), count in by_site.most_common():
        lines.append(f"| {count} | {tool} | {kind} | `{site}` |")

    lines.append("")
    for key, count in by_site.most_common():
        common = ", ".join(f"`{where}`" for where, _ in sources[key].most_common(4))
        if common:
            lines.append(f"- `{key[2]}` ({count}): {common}")

    return "\n".join(lines) + "\n"


def main() -> int:
    """Writes the grouped summary to stdout and to GITHUB_STEP_SUMMARY."""
    parser = argparse.ArgumentParser(
        prog="summarise_sanitizer_reports",
        description="Groups a run's sanitizer findings by the site that produced them.",
    )
    parser.add_argument("paths", nargs="+", help="Report files or directories holding them.")
    args = parser.parse_args()

    text = []
    for raw in args.paths:
        path = Path(raw)
        if path.is_dir():
            for child in sorted(path.rglob("*")):
                if child.is_file():
                    text.append(child.read_text(encoding="utf-8", errors="replace"))
        elif path.is_file():
            text.append(path.read_text(encoding="utf-8", errors="replace"))

    report = summarise(findings("\n".join(text)))
    print(report, end="")

    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as handle:
            handle.write(report)

    return 0


if __name__ == "__main__":
    sys.exit(main())
