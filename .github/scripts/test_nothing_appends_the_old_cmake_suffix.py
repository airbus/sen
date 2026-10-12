# === test_nothing_appends_the_old_cmake_suffix.py =====================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that nothing hands a prefix path the `/cmake` suffix the config no longer sits under.

The config installs where CMake's search reaches it from the prefix, so a prefix carrying the suffix
no longer finds Sen. Moving it made every such instruction wrong, and a sweep for the literal
`cmake/sen` found none of them: two pages spell it `$ENV{SEN_PREFIX}/cmake`, one `<prefix>/cmake`,
and the fifth was the scaffolding template, which writes the line into every package `sen package`
generates.

Tracked files rather than `docs/` alone, because that template is the one that mattered most and is
not documentation. Two paths are exempt and say why below.
"""

import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# The installer names both locations deliberately: one installer installs every release, and a
# release predating the move installed the config under <prefix>/cmake/sen.
INSTALLER = "resources/installer/"
# This file quotes the instruction it forbids, in the guards below.
SELF = "/".join(Path(__file__).relative_to(ROOT).parts)

APPENDS_SUFFIX = re.compile(r"CMAKE_PREFIX_PATH[^\n]*?/cmake(?![/\w])")


def tracked_text_files() -> list[str]:
    """Every tracked path, from git rather than a walk, so build output cannot appear."""
    listed = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, capture_output=True, text=True, check=True)
    return [p for p in listed.stdout.split("\0") if p]


def offending_lines() -> list[str]:
    """Every tracked line that hands a prefix path the old suffix."""
    found = []
    for name in tracked_text_files():
        if name.startswith(INSTALLER) or name == SELF:
            continue
        try:
            text = (ROOT / name).read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for number, line in enumerate(text.splitlines(), start=1):
            if APPENDS_SUFFIX.search(line):
                found.append(f"{name}:{number}: {line.strip()}")
    return found


def test_the_pattern_matches_the_instruction_it_is_for():
    """Passes either way. Without this the check could pass by matching nothing at all."""
    assert APPENDS_SUFFIX.search('list(APPEND CMAKE_PREFIX_PATH "$ENV{SEN_PREFIX}/cmake")')
    assert APPENDS_SUFFIX.search("point `CMAKE_PREFIX_PATH` at `<prefix>/cmake` to resolve it")


def test_the_pattern_leaves_the_current_form_alone():
    """Also passes either way, and bounds it: what the tree should now say must not match."""
    assert not APPENDS_SUFFIX.search('list(APPEND CMAKE_PREFIX_PATH "$ENV{SEN_PREFIX}")')
    assert not APPENDS_SUFFIX.search("CMAKE_PREFIX_PATH reaches <prefix>/lib/cmake/sen from the prefix")


def test_the_installer_is_the_only_exemption():
    """The exemption has to name something real, or it is quietly covering nothing."""
    installer = [n for n in tracked_text_files() if n.startswith(INSTALLER)]
    assert installer, "the installer exemption names a path that does not exist"


def test_nothing_appends_the_suffix():
    """The claim: nobody following the tree builds a prefix path that cannot find Sen."""
    offending = offending_lines()
    assert not offending, "these still append the pre-move suffix:\n" + "\n".join(offending)
