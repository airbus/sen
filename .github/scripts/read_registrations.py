# === read_registrations.py ============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Reads which CMakeLists registers each test that has no source macro.

A smoke or integration test is declared in CMake rather than in a TEST macro, so it has no
suite to group it by and no `/// @test` block to describe it. Where it was registered is the
one piece of structure it does carry, and it is enough to put it beside its own component
instead of in one undifferentiated list.
"""

import re
from pathlib import Path

# The name may sit on the line after the opening parenthesis, which is how the longer calls are
# written, so the pattern crosses newlines. A name built from a variable is skipped: resolving it
# would mean evaluating the CMake.
# Any add_*test* command, rather than a list of the helpers that exist today: the tree has several
# and naming them one by one is how the first version of this missed 69 tests. Over-collecting is
# harmless, since a name that is not a registered test matches no case.
CALL = re.compile(
    r"^\s*(add_[a-z0-9_]*test[a-z0-9_]*)\s*\(\s*(?:NAME\s+)?([A-Za-z0-9_]+(?:\$\{[A-Za-z0-9_]+\})?)",
    re.M | re.S,
)

# A name built in a loop, "term_session_${_case}", cannot be resolved without evaluating the CMake.
# Its literal part is still a real prefix written in that file, so it is kept as one. Short prefixes
# are dropped: they would claim tests registered elsewhere.
MINIMUM_PREFIX = 5


# CMake writes one of these per directory it registered a test in, naming every test exactly and
# saying which source directory it came from. That is the same thing ctest reads, so it cannot
# disagree with the run.
CTEST_FILE = "CTestTestfile.cmake"
CTEST_NAME = re.compile(r"^\s*add_test\(\s*\[=*\[(.*?)\]=*\]", re.M)
CTEST_SOURCE = re.compile(r"^# Source directory:\s*(.*)$", re.M)


def scan_ctest(build: Path, root: Path) -> dict[str, str]:
    """Every registered test, from the files CMake generated for ctest.

    The CMakeLists scan below has to guess at a name built from variables, and a test whose name
    is nothing but variables it cannot place at all. These files have already evaluated the
    build, so they name every test outright.
    """
    found: dict[str, str] = {}
    if not build.is_dir():
        return found

    for path in sorted(build.rglob(CTEST_FILE)):
        text = path.read_text(encoding="utf-8", errors="replace")
        source = CTEST_SOURCE.search(text)
        if source is None:
            continue
        try:
            where = Path(source.group(1).strip()).resolve().relative_to(root.resolve()).as_posix()
        except ValueError:
            # Generated outside the repository, so it says nothing about where a test lives.
            continue
        for name in CTEST_NAME.findall(text):
            found.setdefault(name, where)

    return found


class Registrations:
    """Where each CMake-registered test was declared."""

    def __init__(self) -> None:
        """Starts with nothing registered."""
        self.exact: dict[str, str] = {}
        self.prefixes: list[tuple[str, str]] = []

    def add(self, name: str, directory: str) -> None:
        """Records one registration, literal or generated."""
        if "${" not in name:
            self.exact[name] = directory
            return

        prefix = name.split("${", 1)[0]
        if len(prefix) >= MINIMUM_PREFIX:
            self.prefixes.append((prefix, directory))

    def directory_of(self, name: str) -> str:
        """The directory that registered a test, or empty if none claims it."""
        exact = self.exact.get(name)
        if exact is not None:
            return exact

        # Longest prefix wins, so a more specific family beats a broader one.
        matches = [(len(p), d) for p, d in self.prefixes if name.startswith(p)]
        return max(matches)[1] if matches else ""

    def __len__(self) -> int:
        return len(self.exact) + len(self.prefixes)


def scan(root: Path) -> Registrations:
    """Every CMake-registered test under root, by the directory that registers it."""
    found = Registrations()
    for path in sorted(root.rglob("CMakeLists.txt")):
        if "build" in path.parts or ".git" in path.parts:
            continue
        directory = str(path.parent).removeprefix("./")
        for call in CALL.finditer(path.read_text(encoding="utf-8", errors="replace")):
            found.add(call.group(2), directory)
    return found
