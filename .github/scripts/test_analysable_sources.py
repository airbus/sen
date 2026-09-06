# === test_analysable_sources.py =======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the filter that keeps headers out of the changed-lines clang-tidy lane.

A header has no entry in the compile database, so clang-tidy falls back to a bare invocation and
reports every symbol in it as unprovided. That failed the lane on its own pull request.
"""

import json

from analysable_sources import analysable


def write_database(tmp_path, files):
    """Writes a compile database naming the given files, and returns its path."""
    database = tmp_path / "compile_commands.json"
    database.write_text(
        json.dumps([{"directory": str(tmp_path), "file": str(f), "command": "clang++ -c"} for f in files]),
        encoding="utf-8",
    )
    return str(database)


def test_keeps_a_source_the_database_names(tmp_path):
    """A translation unit the database has a command for is analysable."""
    source = tmp_path / "a.cpp"
    source.touch()
    database = write_database(tmp_path, [source])

    assert analysable(database, [str(source)]) == [str(source)]


def test_drops_a_header_the_database_does_not_name(tmp_path):
    """The case that failed the lane: a changed header has no compile command."""
    source = tmp_path / "a.cpp"
    header = tmp_path / "a.h"
    source.touch()
    header.touch()
    database = write_database(tmp_path, [source])

    assert analysable(database, [str(header)]) == []


def test_keeps_the_source_and_drops_the_header_together(tmp_path):
    """A mixed change keeps what can be analysed rather than rejecting the lot."""
    source = tmp_path / "a.cpp"
    header = tmp_path / "a.h"
    source.touch()
    header.touch()
    database = write_database(tmp_path, [source])

    assert analysable(database, [str(header), str(source)]) == [str(source)]


def test_resolves_a_relative_entry_against_its_directory(tmp_path):
    """A database may name a file relative to the entry's directory."""
    source = tmp_path / "a.cpp"
    source.touch()
    database = tmp_path / "compile_commands.json"
    database.write_text(
        json.dumps([{"directory": str(tmp_path), "file": "a.cpp", "command": "clang++ -c"}]),
        encoding="utf-8",
    )

    assert analysable(str(database), [str(source)]) == [str(source)]


def test_matches_the_same_file_reached_by_a_different_route(tmp_path):
    """`a/../a.cpp` and `a.cpp` are one translation unit, not two."""
    source = tmp_path / "a.cpp"
    source.touch()
    database = write_database(tmp_path, [source])
    indirect = str(tmp_path / "sub" / ".." / "a.cpp")
    (tmp_path / "sub").mkdir()

    assert analysable(database, [indirect]) == [indirect]


def test_returns_nothing_when_no_changed_file_is_a_translation_unit(tmp_path):
    """The lane must pass rather than analyse nothing and call it clean."""
    source = tmp_path / "a.cpp"
    source.touch()
    database = write_database(tmp_path, [source])

    assert analysable(database, [str(tmp_path / "README.md")]) == []
