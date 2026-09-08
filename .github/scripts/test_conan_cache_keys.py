"""Guard the conan cache key, which is spelled by hand in fourteen places.

A cache is stored under a key and retrieved by asking for the same string. The
restore, the drop and the save of one cache live in different steps and often in
different files, and the composite action builds its key from `inputs.*` while
the workflows build the same key from `matrix.*` -- different text that has to
produce identical results.

A divergence here is the quiet kind. Nothing fails: the writer stores under one
string, the reader asks for another, they never meet, and the lane rebuilds its
dependencies from source for twenty to thirty minutes. A slow build looks like a
slow build. `prepare_build` and the merge path disagreed by one trailing dash
for an unknown length of time and it was found by accident.

What this does NOT check: that the keys are correct. Fourteen sites agreeing on
the wrong key pass. It checks they have not drifted apart.
"""

import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCES = sorted((ROOT / ".github").rglob("*.yaml"))

# A key runs to the end of the line or to the quote that closes it. It cannot stop
# at whitespace: `${{ hashFiles('tools/ci/Dockerfile') }}` contains spaces.
KEY = re.compile(r"conanp-[^\n\"]*")
EXPRESSION = re.compile(r"\$\{\{\s*(.+?)\s*\}\}")

# The same slot, named differently depending on whether a composite action or a
# workflow is doing the spelling. Anything not listed here fails the run rather
# than being normalised to something plausible: an unrecognised expression is
# exactly how a new spelling would slip in unnoticed.
SLOTS = {
    "inputs.runner-label": "RUNNER",
    "matrix.runner": "RUNNER",
    "inputs.compiler-name": "COMPILER",
    "matrix.compiler.name": "COMPILER",
    "inputs.compiler-version": "COMPILER_VERSION",
    "matrix.compiler.version": "COMPILER_VERSION",
    "inputs.std": "STD",
    "matrix.std": "STD",
    "matrix.build_type": "BUILD_TYPE",
    "hashFiles('tools/ci/Dockerfile')": "IMAGE",
}


def shape(key: str) -> tuple[str, list[str]]:
    """The key with each expression replaced by its slot, plus any it did not know."""
    unknown: list[str] = []

    def slot(match: re.Match[str]) -> str:
        expression = match.group(1)
        if expression not in SLOTS:
            unknown.append(expression)
            return "{?}"
        return "{" + SLOTS[expression] + "}"

    return EXPRESSION.sub(slot, key).rstrip(), unknown


def keys() -> dict[str, list[str]]:
    """Maps a normalised key shape -> the places that spell it."""
    found: dict[str, list[str]] = defaultdict(list)
    for path in SOURCES:
        for raw in KEY.findall(path.read_text(encoding="utf-8")):
            normalised, _ = shape(raw)
            found[normalised].append(str(path.relative_to(ROOT)))
    return found


def test_every_expression_in_a_cache_key_is_recognised():
    """An unrecognised expression means a new spelling nobody compared."""
    strangers = []
    for path in SOURCES:
        for raw in KEY.findall(path.read_text(encoding="utf-8")):
            _, unknown = shape(raw)
            for expression in unknown:
                strangers.append(f"{expression} in {path.relative_to(ROOT)}")

    assert not strangers, (
        "a cache key uses an expression this guard does not know, so it cannot "
        "tell whether it agrees with the others -- add it to SLOTS: " + "; ".join(sorted(set(strangers)))
    )


def test_the_key_families_are_the_ones_this_repository_uses():
    """A fourteenth spelling that differs from the other thirteen shows up as a new shape.

    Pinned deliberately. Adding a family is a real change and should require
    saying so here, which is the moment somebody checks the reader and the
    writer still agree.
    """
    expected = {
        "conanp-{RUNNER}-{COMPILER}-{COMPILER_VERSION}-{STD}",
        "conanp-{RUNNER}-{COMPILER}-{COMPILER_VERSION}-",  # prepare_build's restore ladder
        # Hash last, so a prefix can reach the family; the bare prefixes are the
        # restore ladders that reach the previous image's entry.
        "conanp-image-docs-{IMAGE}",
        "conanp-image-docs-",
        "conanp-image-clang-20-17-{IMAGE}",
        "conanp-image-clang-20-17-",
    }
    found = set(keys())
    assert found == expected, (
        "the set of conan cache key shapes changed -- new: "
        f"{sorted(found - expected)}; gone: {sorted(expected - found)}"
    )


def test_each_family_is_spelled_in_more_than_one_place():
    """Without this a rename would empty the scan and every assertion above would pass.

    The families that matter are written by one step and read by another; a
    family found in a single file is one nobody is comparing.
    """
    found = keys()
    for family in ("conanp-{RUNNER}-{COMPILER}-{COMPILER_VERSION}-{STD}", "conanp-image-docs-{IMAGE}"):
        assert family in found, f"{family} is no longer found by the key scan"
        assert len(found[family]) >= 2, f"{family} is spelled once, in {found[family]}, so nothing is compared"
