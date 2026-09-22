"""Checks that a workflow building several build types tells conan which one.

Nothing else carries it: conan takes the profile's type, so a matrix leg that only
names its type in the output path builds Release and looks for it somewhere else.
That is how the two debug-information legs failed on the 0.0.0-rc1 tag.
"""

import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = sorted((ROOT / ".github" / "workflows").glob("*.yaml"))
CONAN = re.compile(r"^\s*conan (?:install|build) \.", re.MULTILINE)
VARYING = [p for p in WORKFLOWS if "matrix.build_type" in p.read_text(encoding="utf-8")]


def test_a_workflow_varies_the_build_type():
    """Fails when nothing varies it, which would leave the check below with no subject."""
    assert VARYING, "no workflow builds more than one build type"


@pytest.mark.parametrize("path", VARYING, ids=lambda p: p.name)
def test_every_build_names_its_build_type(path):
    """Fails on a conan command that leaves the build type to the profile."""
    lines = path.read_text(encoding="utf-8").splitlines()
    for start, line in enumerate(lines, start=1):
        if not CONAN.match(line):
            continue
        # A command can be continued, so the setting may sit on a later line.
        command, index = line, start
        while command.rstrip().endswith("\\") and index < len(lines):
            command += lines[index]
            index += 1
        assert "build_type=" in command, f"{path.name}:{start} builds without naming a build type"
