# === test_read_registrations.py =======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that a registration is placed by where it sits in the tree, not by where the tree sits.

The document reads the area off the leading path segments, so a scan given an absolute root once
put every test under the first two segments of the checkout path. The workflow passes a relative
root and never saw it; a reader rendering the same document by hand does.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import read_registrations  # noqa: E402


def write_tree(root: Path) -> None:
    """A registration two directories down, which is as deep as an area reaches."""
    where = root / "libs" / "core" / "test"
    where.mkdir(parents=True)
    (where / "CMakeLists.txt").write_text("add_test(NAME core_lang_test COMMAND core_lang_test)\n")


def test_the_directory_is_relative_to_the_root(tmp_path: Path) -> None:
    """The two leading segments are the area, so they have to be the tree's own."""
    write_tree(tmp_path)
    found = read_registrations.scan(tmp_path)
    assert found.directory_of("core_lang_test") == "libs/core/test"


def test_an_absolute_root_reads_the_same_as_a_relative_one(tmp_path: Path, monkeypatch) -> None:
    """The workflow passes a relative root; anyone rendering by hand passes an absolute one."""
    write_tree(tmp_path)
    monkeypatch.chdir(tmp_path)
    assert read_registrations.scan(Path(".")).directory_of("core_lang_test") == (
        read_registrations.scan(tmp_path).directory_of("core_lang_test")
    )


def test_a_registration_at_the_root_has_no_directory(tmp_path: Path) -> None:
    """The root resolves to "." as a path, which is not a segment of any area."""
    (tmp_path / "CMakeLists.txt").write_text("add_test(NAME top_test COMMAND top_test)\n")
    assert not read_registrations.scan(tmp_path).directory_of("top_test")
