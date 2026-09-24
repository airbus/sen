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
