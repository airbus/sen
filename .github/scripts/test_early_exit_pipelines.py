"""Checks that nothing pipes into `grep -q`.

grep -q leaves as soon as it matches. Whatever is writing then dies on the closed
pipe, and under `pipefail` -- which is what `shell: bash` gives a workflow step --
that becomes the pipeline's status. So the check reports failure because it found
what it was looking for, or takes the wrong branch and reports nothing at all.

It needs output past the pipe buffer to bite, so a small case passes against the
broken form and the smallest inputs never show it. That is why this reads the
sources rather than running anything: the size that triggers it is a property of
the day's data, not of the code.

Found on 2026-09-22 in the standard tests, in clang_tidy_diff.yaml where it would
have skipped the full sweep on a pull request that edits .clang-tidy, and in
drop_cache_key.sh. The replacement is a herestring, which has no pipe to close.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCES = sorted(
    [*(ROOT / ".github").rglob("*.yaml"), *(ROOT / ".github").rglob("*.sh"), *(ROOT / "tools").rglob("*.sh")]
)
# `-q` among the short options of a grep on the right of a pipe.
PIPED_INTO_QUIET_GREP = re.compile(r"\|\s*grep\s+(?:-\w*\s+)*-\w*q")


def offenders() -> list[str]:
    """Returns 'path:line: text' for every pipe into a grep that leaves early."""
    found = []
    for path in SOURCES:
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            if PIPED_INTO_QUIET_GREP.search(line):
                found.append(f"{path.relative_to(ROOT)}:{number}: {line.strip()}")
    return found


def test_the_scan_sees_one():
    """The pattern matches the shape, so an empty result below means absence."""
    assert PIPED_INTO_QUIET_GREP.search("ctest -N -L integration | grep -q object_sync")
    assert PIPED_INTO_QUIET_GREP.search("git diff --name-only HEAD | grep -qxF '.clang-tidy'")
    assert not PIPED_INTO_QUIET_GREP.search('grep -q object_sync <<<"$registered"')


def test_nothing_pipes_into_a_grep_that_leaves_early():
    """Use a herestring instead; there is then no pipe for grep to close."""
    assert not offenders(), "pipe into grep -q: " + "; ".join(offenders())
