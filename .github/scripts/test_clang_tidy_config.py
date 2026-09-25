# === test_clang_tidy_config.py ========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that every entry in `.clang-tidy` can name a check.

`Checks:` is a folded scalar, so newlines become spaces and a `#` is data. A missing comma
therefore merges two checks into one glob, and a comment merges with the entry below it.
clang-tidy accepts both silently: an unmatched glob disables nothing and reports nothing,
so the check stops running. Three in this file had, from the initial commit until 2026-09-24.
"""

import re
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
