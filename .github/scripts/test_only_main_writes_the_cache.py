# === test_only_main_writes_the_cache.py ===============================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that only a run on main writes anything outside its own run.

The repository's cache budget is 10 GiB, shared by every entry and not raisable, so a branch
writing into it takes room from the lanes that need it. Every save in the repository is gated on
`refs/heads/main` for that reason, and so is every drop.

The drop is the half that costs. A cache key is immutable, so a save under an existing key is
refused and the drop exists to clear the way; a drop on a branch therefore deletes what main
wrote and leaves its own content in place. Of nineteen cache writes, the clang-tidy result cache
is the one where both halves need the gate.

Restoring is deliberately left open to every ref, so a branch still reads what main built.

Nothing fails when a gate is missing. The branch's run is green, the entry is replaced, and the
cost lands on a later run of a different workflow that finds a cache it did not write.

The same question has to be asked of every outward write, not only the cache. Asking it of the
cache alone is how `publish-coverage` was missed on the same day: it pushes to `gh-pages`, the
public documentation site, and was gated only on the site existing, so a dispatch on a branch
replaced main's published coverage report with a branch's (gh-pages 0cfecdce0, 2026-10-11).
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCES = sorted((ROOT / ".github" / "workflows").glob("*.yaml")) + sorted(
    (ROOT / ".github" / "actions").glob("*/action.yaml")
)
# A step that writes: `actions/cache/save`, or the helper that deletes an entry to make way.
WRITER = re.compile(r"actions/cache/save|drop_cache_key\.sh")
ON_MAIN = "refs/heads/main"


def writers() -> list[tuple[Path, int, str]]:
    """Every cache write, with the condition of the step it sits in."""
    found = []
    for source in SOURCES:
        lines = source.read_text(encoding="utf-8").splitlines()
        for number, line in enumerate(lines, 1):
            if not WRITER.search(line):
                continue
            condition = ""
            for earlier in range(number - 1, max(0, number - 11), -1):
                match = re.match(r"\s+if:\s*(.*)", lines[earlier - 1])
                if match:
                    condition = match.group(1)
                    break
            found.append((source, number, condition))
    return found


# The job that replaces the published coverage report. Scoped to that one rather than to
# gh-pages in general: build_docs.yaml deploys through `mike deploy --push` and does so per ref
# on purpose, keeping a versioned directory for each, so "main only" is the wrong rule there --
# and it takes no workflow_dispatch, so a branch cannot reach it the way this one was reached.


def jobs_that_publish() -> list[tuple[Path, str, str]]:
    """Every job that replaces the published report, with the condition guarding the job."""
    found = []
    for source in SOURCES:
        text = source.read_text(encoding="utf-8")
        for match in re.finditer(r"^  ([a-z][a-z0-9-]*):\n((?:    .*\n|\n)*)", text, re.M):
            name, body = match.group(1), match.group(2)
            if "HEAD:gh-pages" not in body and "push origin gh-pages" not in body:
                continue
            condition = re.search(r"^    if:\s*(.*)$", body, re.M)
            found.append((source, name, condition.group(1) if condition else ""))
    return found


def test_the_published_report_is_replaced_only_from_main():
    """It overwrites what main published, and the page itself says nothing about the ref.

    The audit above asks what a branch dispatch writes to the cache; this asks what it writes
    at all.
    """
    ungated = [f"{source.name}:{job}" for source, job, condition in jobs_that_publish() if ON_MAIN not in condition]
    assert not ungated, f"jobs pushing to gh-pages without a {ON_MAIN} gate: {ungated}"


def test_there_are_cache_writes_to_check():
    """Fails if the helper is renamed, which would leave the gate below guarding nothing."""
    assert len(writers()) >= 15, f"only {len(writers())} cache writes found; has the pattern changed?"


def test_every_cache_write_is_gated_on_main():
    """A branch that writes takes room from the budget, and a branch that drops takes main's entry."""
    ungated = [f"{source.name}:{number}" for source, number, condition in writers() if ON_MAIN not in condition]
    assert not ungated, f"cache writes not gated on {ON_MAIN}: {ungated}"
