# === test_conan_lockfile_is_explicit.py ===============================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that every conan invocation in CI names the lockfile.

The lockfile used to sit at the repository root, where conan loads it for any command run there.
That pinned a consumer's resolution to recipe revisions their own remote may not carry, and the way
out was documented nowhere. It now lives under `.conan/`, which conan does not read by itself, so
each CI invocation has to ask for it. Miss one and that lane resolves unpinned, silently and green.

Commands printed in the documentation are deliberately not covered: a reader building from a clone
should resolve for themselves rather than inherit CI's pinned revisions.
"""

import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
LOCKFILE = ROOT / ".conan" / "conan.lock"

# An invocation, not prose about one: a comment line is skipped before this is applied. `build` is
# here because it regenerates the toolchain and compiles, so it resolves a graph of its own; `graph`
# because the dependency-alignment check reads one.
INVOCATION = re.compile(r"\bconan\s+(?:install|create|test|build|graph)\s+\S")

# The flag has to name our lockfile. conan documents --lockfile="" as the way to opt out, and
# --lockfile-out= is an output, so looking for "--lockfile" alone accepts both.
PINNED = "--lockfile=.conan/conan.lock"


def ci_files() -> list[Path]:
    """The workflow and composite-action files, which is where CI runs conan."""
    found = sorted((ROOT / ".github" / "workflows").glob("*.yaml"))
    found += sorted((ROOT / ".github" / "actions").glob("*/action.yaml"))
    return found


def joined_lines(text: str) -> list[str]:
    """Shell continuations joined, so a flag on the next line still belongs to its command."""
    joined: list[str] = []
    for line in text.splitlines():
        stripped = line.strip()
        if joined and joined[-1].endswith("\\"):
            joined[-1] = joined[-1][:-1].rstrip() + " " + stripped
        else:
            joined.append(stripped)
    return joined


def invocations(text: str) -> list[str]:
    """Commands that invoke conan, with comments and prose left out."""
    return [
        line
        for line in joined_lines(text)
        if not line.startswith("#") and INVOCATION.search(line) and "conan inspect" not in line
    ]


def test_the_lockfile_is_where_conan_will_not_find_it_by_itself():
    """The whole point: at the repository root conan would load it for a consumer unasked."""
    assert LOCKFILE.is_file(), f"{LOCKFILE} is missing"
    assert not (ROOT / "conan.lock").exists(), "a root conan.lock is loaded by conan for any command run there"


def test_every_ci_invocation_names_the_lockfile():
    """A lane that resolves unpinned still passes, so nothing else would catch this."""
    offenders = []
    for path in ci_files():
        for line in invocations(path.read_text(encoding="utf-8")):
            if PINNED not in line:
                offenders.append(f"{path.relative_to(ROOT)}: {line}")
    assert not offenders, "conan invocations resolving without the lockfile:\n" + "\n".join(offenders)


def test_the_dependency_alignment_check_is_pinned():
    """Conan reads a lockfile beside the recipe, and this script runs from the repository root.

    Unpinned it resolves the newest revision of everything and validates a graph no lane builds, so
    an upstream publication alone can turn the check red with nothing changed here.
    """
    script = (ROOT / ".github" / "scripts" / "check_dependency_alignment.py").read_text(encoding="utf-8")
    assert "conan" in script and "graph" in script, "this test is pointed at the wrong script"
    assert "--lockfile=" in script, "check_dependency_alignment.py resolves its graph unpinned"


def test_invocations_were_actually_found():
    """The test above passes over an empty list, so the scan has to be shown to see something."""
    total = sum(len(invocations(path.read_text(encoding="utf-8"))) for path in ci_files())
    assert total >= 10, f"only found {total} conan invocations; the scan has stopped seeing them"


@pytest.mark.parametrize(
    ("line", "found"),
    [
        ("          conan install . -s build_type=Release", True),
        ("          conan create . --user airbus", True),
        ('          conan test .conan/test_packages/package "sen/1.0@airbus/dev"', True),
        ("          # documented `conan install --profile=...` work here", False),
        ("          conan inspect . --format=json", False),
        ("          cmake --build .", False),
        ("          conan build . -s build_type=Release", True),
        ("          conan graph info . --format=json", True),
    ],
    ids=["install", "create", "test", "a-comment", "another-subcommand", "not-conan", "build", "graph"],
)
def test_the_scan_tells_an_invocation_from_prose(line, found):
    """A scan that counted comments would report offenders nobody can fix."""
    assert bool(invocations(line)) is found


@pytest.mark.parametrize(
    "flag",
    ['--lockfile=""', "--lockfile=/dev/null", "--lockfile=package.lock", "--lockfile-out=package.lock"],
    ids=["opt-out", "devnull", "another-lockfile", "an-output-not-an-input"],
)
def test_a_flag_that_does_not_name_our_lockfile_is_not_enough(flag):
    """Conan treats --lockfile="" as opting out, so the substring --lockfile proves nothing."""
    assert PINNED not in f"conan install . {flag}"


def test_a_flag_on_a_continuation_line_still_counts():
    """These commands routinely span lines, and a line-at-a-time scan reports a false offender."""
    command = "          conan test .conan/test_packages/package \\\n              --lockfile=x --lockfile-partial"
    found = invocations(command)
    assert len(found) == 1, found
    assert "--lockfile=" in found[0], found[0]
