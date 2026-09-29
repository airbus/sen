# === test_clang_tidy_config.py ========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks `.clang-tidy`'s check list and the header filter's several copies.

`Checks:` is a folded scalar, so newlines become spaces and a `#` is data. A missing comma
therefore merges two checks into one glob, and a comment merges with the entry below it.
clang-tidy accepts both silently: an unmatched glob disables nothing and reports nothing,
so the check stops running. Three in this file had, from the initial commit until 2026-09-24.

The header filter is a second subject. It is copied into the compiler-driven build and into the
whole-tree workflow, and a copy left behind narrows what one lane reports without failing anywhere.
"""

import re
import subprocess
from pathlib import Path

import pytest
import yaml

ROOT = Path(__file__).resolve().parents[2]
CONFIG = ROOT / ".clang-tidy"


def unmatchable_entries(text: str) -> list[str]:
    """Returns the entries that no check name can match."""
    checks = yaml.safe_load(text)["Checks"]
    return [entry for raw in checks.split(",") if (entry := raw.strip()) and (" " in entry or "#" in entry)]


def test_every_check_entry_can_match():
    """An entry carrying a space or a `#` silently disables whatever it was meant to enable."""
    found = unmatchable_entries(CONFIG.read_text(encoding="utf-8"))
    assert not found, (
        f".clang-tidy has entries that match no check: {found}. A missing comma joins two "
        f"entries, and a comment inside the folded scalar joins the one below it."
    )


@pytest.mark.parametrize(
    ("planted", "expected"),
    [
        ("Checks: >\n  bugprone-assert-side-effect\n  bugprone-bad-signal-to-kill-thread,\n", 1),
        ("Checks: >\n  # llvm-header-guard,  # enable later\n  misc-misplaced-const,\n", 2),
    ],
    ids=["missing-comma", "comment-inside-the-scalar"],
)
def test_the_scan_sees_both_shapes(planted, expected):
    """Without this the test above passes on a scan that looks at nothing."""
    assert len(unmatchable_entries(planted)) == expected


def test_a_clean_config_reports_nothing():
    """The other half: the scan has to stay quiet on a file with neither defect."""
    assert unmatchable_entries("Checks: >\n  bugprone-*,\n  -modernize-use-trailing-return-type,\n") == []


# Paths as clang-tidy sees them: ours under the checkout, dependencies under conan's cache.
OURS = (
    "/ws/libs/core/include/sen/core/base/hash32.h",
    "/ws/components/shell/include/shell.h",
    "/ws/apps/cli_sen/main.h",
    "/ws/test/support/helper.h",
)
THEIRS = (
    "/conan/p/b/imgui22d862702e925/p/include/../res/bindings/imgui_impl_sdl2.h",
    "/conan/p/asio1234/p/include/asio/io_context.hpp",
)


def header_filter() -> re.Pattern:
    """The regex clang-tidy applies to decide which headers it reports on."""
    return re.compile(yaml.safe_load(CONFIG.read_text(encoding="utf-8"))["HeaderFilterRegex"])


@pytest.mark.parametrize("path", OURS, ids=lambda p: p.split("/")[2])
def test_our_headers_are_analysed(path):
    """Narrowing the filter until it misses our own headers would disable the header checks."""
    assert header_filter().match(path)


@pytest.mark.parametrize("path", THEIRS, ids=lambda p: p.split("/")[3])
def test_dependency_headers_are_not_analysed(path):
    """`.*` reported findings in a conan package header, and WarningsAsErrors made them fatal."""
    assert not header_filter().match(path)


# The filter lives in several places: `.clang-tidy` for a bare clang-tidy run, a cmake variable for
# the compiler-driven build, and the workflow for the whole-tree sweep, which passes it twice, once
# to pick the headers to report on and once to pick the sources to analyse.
FILTER_FILES = (
    ".clang-tidy",
    "cmake/util/sen_internal_utils.cmake",
    ".github/workflows/clang_tidy_diff.yaml",
)
EXPECTED_COPIES = {
    ".clang-tidy": 1,
    "cmake/util/sen_internal_utils.cmake": 1,
    ".github/workflows/clang_tidy_diff.yaml": 2,
}

# `.clang-tidy` travels with the checkout, whose path differs per machine and per container, so it
# is the one copy that cannot name a root. The others interpolate one and are anchored to it.
UNROOTED = ".clang-tidy"

# Top-level directories with tracked C++ that the filter leaves out. `.conan` and `examples` build
# against the install tree rather than with the sweep; `cmake` holds the sanitizer options shim.
EXCLUDED_SOURCE_DIRS = {".conan", "cmake", "examples"}

# A parenthesised alternation of lowercase words, with the surrounding expression. Naming real
# directories is what marks one as a path filter, so `(h|hpp|hxx)` and `(warning|error):` stay out.
FILTER_TOKEN = re.compile(r"""([^\s"';]*\(([a-z_]+(?:\|[a-z_]+)+)\)[^\s"';]*)""")


def git_ls(*patterns: str) -> list[str]:
    """Tracked paths, so an untracked build directory cannot answer for the repository layout."""
    listing = subprocess.run(
        ["git", "-C", str(ROOT), "ls-files", *patterns], capture_output=True, text=True, check=True
    )
    return listing.stdout.split()


def top_level_dirs() -> set[str]:
    """The repository's own directories, which is what tells a path filter from any other regex."""
    return {path.split("/")[0] for path in git_ls() if "/" in path}


def header_filters(text: str, real_dirs: set[str]) -> list[tuple[str, frozenset[str]]]:
    """Returns each path filter in `text` as its whole expression and the directories it names."""
    found = []
    for expression, alternation in FILTER_TOKEN.findall(text):
        named = frozenset(alternation.split("|"))
        if len(named & real_dirs) >= 2:
            found.append((expression, named))
    return found


def is_rooted(expression: str) -> bool:
    """A filter anchored to a root cannot reach outside the tree it names."""
    return expression.startswith("^")


def group_follows_separator(expression: str) -> bool:
    """Without the separator the filter also matches a directory merely ending in the name."""
    return "/(" in expression


def copy_params(rooted_only: bool = False) -> list:
    """One pytest case per copy, named for its file and its position in it."""
    params: list = []
    seen: dict[str, int] = {}
    for rel, expression, _ in copies():
        if rooted_only and rel == UNROOTED:
            continue
        name = rel.split("/")[-1]
        seen[name] = seen.get(name, 0) + 1
        params.append(pytest.param(rel, expression, id=f"{name}#{seen[name]}"))
    return params


def copies() -> list[tuple[str, str, frozenset[str]]]:
    """Every copy of the filter as its file, its whole expression, and the directories it names."""
    real_dirs = top_level_dirs()
    return [
        (rel, expression, named)
        for rel in FILTER_FILES
        for expression, named in header_filters((ROOT / rel).read_text(encoding="utf-8"), real_dirs)
    ]


def test_every_copy_is_still_found():
    """A moved or renamed copy would otherwise leave the tests below asserting over less."""
    counted = dict.fromkeys(FILTER_FILES, 0)
    for rel, _, _ in copies():
        counted[rel] += 1
    assert counted == EXPECTED_COPIES, (
        f"the header filter copies have moved: found {counted}, expected {EXPECTED_COPIES}. "
        f"Point FILTER_FILES at the new location rather than lowering the count."
    )


def test_all_copies_name_the_same_directories():
    """A directory added to one copy and not the rest is reported on by one lane only."""
    named = {rel: sorted(dirs) for rel, _, dirs in copies()}
    distinct = {tuple(dirs) for dirs in named.values()}
    assert len(distinct) == 1, f"the header filter copies disagree on which directories to cover: {named}"


@pytest.mark.parametrize(("rel", "expression"), copy_params(rooted_only=True))
def test_copies_that_can_name_a_root_are_anchored(rel, expression):
    """Unanchored, the filter reaches a dependency whose own tree holds a directory of that name."""
    assert is_rooted(expression), f"{rel} has an unanchored filter: {expression}"


@pytest.mark.parametrize(("rel", "expression"), copy_params())
def test_the_directory_group_follows_a_separator(rel, expression):
    """`(libs|...)` without the separator also matches `mylibs/`, ours or a dependency's."""
    assert group_follows_separator(expression), f"{rel} matches a directory ending in the name: {expression}"


def test_left_out_source_directories_are_listed():
    """Adding a top-level directory of C++ reddens this until it is filtered or named here."""
    with_cxx = {path.split("/")[0] for path in git_ls("*.cpp", "*.cc", "*.cxx", "*.h", "*.hpp") if "/" in path}
    covered = next(dirs for _, _, dirs in copies())
    assert with_cxx - covered == EXCLUDED_SOURCE_DIRS, (
        f"top-level directories holding C++ that the filter does not cover: {sorted(with_cxx - covered)}, "
        f"listed as deliberate: {sorted(EXCLUDED_SOURCE_DIRS)}. Add the directory to every copy of the "
        f"filter, or to EXCLUDED_SOURCE_DIRS once it is meant to stay out."
    )


PLANTED_DIRS = {"libs", "components", "apps", "test"}


def test_the_scan_finds_a_planted_copy():
    """Without this the tests above could be reading an empty list and passing."""
    found = header_filters('-header-filter "^$PWD/(libs|components|apps|test)/"', PLANTED_DIRS)
    assert found == [("^$PWD/(libs|components|apps|test)/", frozenset(PLANTED_DIRS))]


@pytest.mark.parametrize(
    "text",
    ["\\.(h|hpp|hxx)$", "grep -nE '(warning|error):'"],
    ids=["header-suffixes", "diagnostic-kinds"],
)
def test_the_scan_ignores_alternations_that_name_no_directory(text):
    """These sit in the same workflow, and counting them would hide a copy going missing."""
    assert header_filters(text, PLANTED_DIRS) == []


def test_the_scan_sees_a_copy_left_behind():
    """The shape the agreement test exists for: one copy narrowed, the others untouched."""
    stale = header_filters("^$PWD/(libs|components|apps)/", PLANTED_DIRS)
    current = header_filters("^$PWD/(libs|components|apps|test)/", PLANTED_DIRS)
    assert stale[0][1] != current[0][1]


@pytest.mark.parametrize(
    ("expression", "rooted", "separated"),
    [
        ("^$PWD/(libs|components|apps|test)/", True, True),
        (".*/(libs|components|apps|test)/.*", False, True),
        ("^$PWD(libs|components|apps|test)/", True, False),
    ],
    ids=["anchored-and-separated", "unanchored", "no-separator"],
)
def test_the_shape_checks_see_both_answers(expression, rooted, separated):
    """Both halves: each check has to reject as well as accept, or it asserts nothing."""
    assert is_rooted(expression) is rooted
    assert group_follows_separator(expression) is separated
