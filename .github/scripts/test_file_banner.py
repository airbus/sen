# === test_file_banner.py ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Tests for the banner check, mostly the cases where it should complain."""

import pathlib
import sys

# The check sits beside this file rather than on the path, as the other script tests here do.
sys.path.insert(0, str(pathlib.Path(__file__).parent))

import check_file_banner as banner  # noqa: E402


def write(tmp_path: pathlib.Path, name: str, text: str) -> pathlib.Path:
    """A file on disk with the given contents, since the check reads by path."""
    p = tmp_path / name
    p.write_text(text, encoding="utf-8")
    return p


def good(name: str, comment: str, rest: str = "") -> str:
    """A correctly bannered file."""
    return "\n".join(banner.expected_banner(name, comment)) + "\n" + rest


def test_a_correct_banner_passes(tmp_path: pathlib.Path) -> None:
    """The shape the repository already uses."""
    assert not banner.check(write(tmp_path, "thing.cpp", good("thing.cpp", "//")))
    assert not banner.check(write(tmp_path, "thing.py", good("thing.py", "#")))


def test_a_missing_banner_is_reported(tmp_path: pathlib.Path) -> None:
    """A new file that opens straight into code."""
    assert banner.check(write(tmp_path, "thing.cpp", "int main() {}\n"))


def test_a_banner_naming_another_file_is_reported(tmp_path: pathlib.Path) -> None:
    """What a rename leaves behind."""
    stale = good("other_name.cpp", "//")
    assert banner.check(write(tmp_path, "thing.cpp", stale))


def test_a_rule_one_column_short_is_reported(tmp_path: pathlib.Path) -> None:
    """The banner is padded to a fixed column, so a short rule is a real difference."""
    lines = banner.expected_banner("thing.cpp", "//")
    lines[-1] = lines[-1][:-1]
    assert banner.check(write(tmp_path, "thing.cpp", "\n".join(lines) + "\n"))


def test_a_shebang_keeps_the_first_line(tmp_path: pathlib.Path) -> None:
    """A script has to stay executable, so the banner follows the shebang."""
    text = "#!/usr/bin/env python3\n" + good("thing.py", "#")
    assert not banner.check(write(tmp_path, "thing.py", text))


def test_an_unknown_suffix_is_left_alone(tmp_path: pathlib.Path) -> None:
    """Markdown would render the banner as text."""
    assert not banner.check(write(tmp_path, "page.md", "# A page\n"))


def test_cmakelists_is_checked_by_name(tmp_path: pathlib.Path) -> None:
    """Its suffix is shared with files that carry no banner, so the name is what selects it."""
    assert not banner.check(write(tmp_path, "CMakeLists.txt", good("CMakeLists.txt", "#")))
    assert banner.check(write(tmp_path, "CMakeLists.txt", "add_sen_package(\n"))


def test_another_txt_is_left_alone(tmp_path: pathlib.Path) -> None:
    """LICENSE.txt and the sanitizer ignorelists share the suffix and must stay untouched."""
    assert not banner.check(write(tmp_path, "LICENSE.txt", "Apache License\n"))
