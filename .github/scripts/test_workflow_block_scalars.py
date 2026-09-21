"""Checks that no comment was written inside a workflow's block scalar.

A `#` line under `restore-keys: |` is a cache key, not a comment. The file stays valid YAML
and actionlint stays quiet, so the first thing that notices is the run: a lane failed with
"Key Validation Error: # Transitional ... cannot contain commas" on 2026-09-21.
"""

import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
# The keys whose value is a literal string a tool then parses, so a stray line becomes data.
BLOCK_KEYS = ("key", "restore-keys", "tags", "build-args", "path", "args", "labels")
SOURCES = sorted(
    list((ROOT / ".github" / "workflows").glob("*.yaml")) + list((ROOT / ".github" / "actions").rglob("*.yaml"))
)


def comment_lines_in_block_scalars(path: Path) -> list[str]:
    """Returns 'line: text' for every comment-shaped line inside one of those blocks."""
    found, indent = [], None
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        opening = re.match(rf"^(\s*)({'|'.join(BLOCK_KEYS)}):\s*[|>]", line)
        if opening:
            indent = len(opening.group(1))
            continue
        if indent is None:
            continue
        if not line.strip():
            continue
        if len(line) - len(line.lstrip()) <= indent:
            indent = None
        elif line.strip().startswith("#"):
            found.append(f"{number}: {line.strip()}")
    return found


@pytest.mark.parametrize("path", SOURCES, ids=lambda p: p.name)
def test_no_comment_inside_a_block_scalar(path):
    """One file per case, so the failure names the file rather than the first of many."""
    found = comment_lines_in_block_scalars(path)
    assert not found, (
        f"{path.relative_to(ROOT)} has a comment inside a block scalar, where it is read as "
        f"a value: {found}. Put it on the line above the key instead."
    )


def test_the_scan_sees_one(tmp_path):
    """Without this the parametrised test passes on an empty scan."""
    planted = tmp_path / "planted.yaml"
    planted.write_text("      restore-keys: |\n        real-key-\n        # not a comment\n")
    assert comment_lines_in_block_scalars(planted) == ["3: # not a comment"]
