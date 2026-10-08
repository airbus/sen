# === read_annotations.py ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Reads the @test and @requirements annotations that sit above a gtest macro.

The repository already annotates its tests:

    /// @test
    /// Validates that creating a named provider returns the same instance.
    /// @requirements(SEN-363)
    TEST(ObjectFilter, NamedProvider_CreateAndRemove)

This reads them so a report can say what each test checks. It is a rendering of the
annotations, not a second place to record them: the test source stays the only mapping,
which is the rule the requirements register sets.
"""

import re
from pathlib import Path
from typing import NamedTuple

# Every macro that declares a suite and a case, which is the handle ctest uses. TYPED_TEST and
# TYPED_TEST_P belong here too: they are registered per type, so one macro becomes several runtime
# cases named Suite.Case<T>.
MACRO = re.compile(
    r"^\s*(TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P)\s*\(\s*([A-Za-z_][\w]*)\s*,\s*([A-Za-z_][\w]*)\s*\)"
)

# The ids in the tree are SEN-nnn, written parenthesised and comma separated. REQ-nnnnn is
# accepted too because the register's own grammar uses it, and a mixed tree should not
# silently drop half its annotations.
# The @ is optional because a line that reads as a requirements annotation without it was
# being folded into the description instead: the requirement was lost and the description
# ended with "requirements(SEN-364)" in it.
REQUIREMENTS = re.compile(r"@?requirements\(([^)]*)\)")
IDENTIFIER = re.compile(r"\b((?:SEN|REQ)-\d+)\b")


class Annotation(NamedTuple):
    """What the source says a test checks, and which requirements it verifies."""

    suite: str
    case: str
    description: str
    requirements: tuple[str, ...]
    # Where it is declared, relative to the repository. A test suite's name says nothing about
    # the part of Sen it covers; the file it sits in does.
    source: str = ""

    @property
    def key(self) -> str:
        """The handle the runner reports, so a result can be joined to this."""
        return f"{self.suite}.{self.case}"


def parse_block(lines: list[str]) -> tuple[str, tuple[str, ...]]:
    """Returns the description and the requirement ids in one comment block."""
    description: list[str] = []
    requirements: list[str] = []
    collecting = False

    for line in lines:
        text = line.strip().removeprefix("///").strip()

        if text.startswith("@test"):
            collecting = True
            rest = text[len("@test") :].strip()
            if rest:
                description.append(rest)
            continue

        found = REQUIREMENTS.search(text)
        if found:
            requirements.extend(IDENTIFIER.findall(found.group(1)))
            collecting = False
            continue

        if text.startswith("@"):
            collecting = False
            continue

        if collecting and text:
            description.append(text)

    return " ".join(description), tuple(dict.fromkeys(requirements))


def scan_file(path: Path, source: str = "") -> list[Annotation]:
    """Every annotated gtest macro in one file."""
    annotations: list[Annotation] = []
    block: list[str] = []

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        stripped = line.strip()

        if stripped.startswith("///"):
            block.append(line)
            continue

        macro = MACRO.match(line)
        if macro:
            description, requirements = parse_block(block)
            annotations.append(Annotation(macro.group(2), macro.group(3), description, requirements, source))

        # Any other line ends the run of comments, so a block always belongs to what follows it.
        if stripped:
            block = []

    return annotations


def scan(root: Path) -> dict[str, Annotation]:
    """Every annotated test under root, keyed by the Suite.Case the runner reports."""
    found: dict[str, Annotation] = {}

    for path in sorted(root.rglob("*.cpp")):
        if "build" in path.parts or ".git" in path.parts:
            continue
        for annotation in scan_file(path, path.relative_to(root).as_posix()):
            found.setdefault(annotation.key, annotation)

    return found
