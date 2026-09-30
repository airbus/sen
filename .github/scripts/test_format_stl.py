# === test_format_stl.py ===============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Tests for the STL member aligner, mostly the layout each rule is meant to produce."""

import pathlib
import subprocess
import sys

# The formatter sits beside this file rather than on the path, as the other script tests here do.
sys.path.insert(0, str(pathlib.Path(__file__).parent))

import format_stl as formatter  # noqa: E402


def test_a_struct_run_gets_one_colon_column() -> None:
    """Members of one compound type line up on the colon."""
    text = "struct S\n{\n  port : u16,\n  hostName : string,\n}\n"
    assert formatter.format_text(text) == "struct S\n{\n  port     : u16,\n  hostName : string,\n}\n"


def test_comments_line_up_two_spaces_past_the_longest_member() -> None:
    """A trailing comment sits two spaces past the longest member of its run."""
    text = "struct S\n{\n  a : u16,  // first\n  bb : string,  // second\n}\n"
    assert formatter.format_text(text) == "struct S\n{\n  a  : u16,     // first\n  bb : string,  // second\n}\n"


def test_an_enumerator_run_has_no_colon_column() -> None:
    """Enum and variant entries carry no type, so only the comment lines up."""
    text = "enum E\n{\n  first,  // one\n  second,  // two\n}\n"
    assert formatter.format_text(text) == "enum E\n{\n  first,   // one\n  second,  // two\n}\n"


def test_attributes_follow_their_member_by_one_space() -> None:
    """[static] and [confirmed] take no column of their own; the colon is the only one."""
    text = "class C\n{\n  var settings : RecordingSettings  [static];\n  var state : RecorderState   [confirmed];\n}\n"
    expected = (
        "class C\n{\n  var settings : RecordingSettings [static];\n  var state    : RecorderState [confirmed];\n}\n"
    )
    assert formatter.format_text(text) == expected


def test_a_lone_member_keeps_its_single_space() -> None:
    """A member held apart by its own doc comment has nothing to line up with."""
    text = "class C\n{\n  // what it is\n  var a : u16;\n\n  // what it is\n  var bbbb : string;\n}\n"
    assert formatter.format_text(text) == text


def test_a_line_holding_a_string_is_left_alone() -> None:
    """A quoted colon would otherwise be read as a member separator and the text would move."""
    text = 'struct S\n{\n  a : string = "host:port",\n  bb : u16,\n}\n'
    assert '"host:port"' in formatter.format_text(text)


def test_only_whitespace_ever_moves() -> None:
    """A file the formatter misreads comes back ugly, never wrong."""
    for path in _repository_stl_files():
        before = path.read_text(encoding="utf-8")
        after = formatter.format_text(before)
        assert "".join(before.split()) == "".join(after.split()), path


def test_formatting_is_idempotent() -> None:
    """A second run must be a no-op, or the hook would never stop rewriting."""
    for path in _repository_stl_files():
        once = formatter.format_text(path.read_text(encoding="utf-8"))
        assert formatter.format_text(once) == once, path


def _repository_stl_files() -> list[pathlib.Path]:
    """Every tracked .stl file."""
    root = pathlib.Path(__file__).resolve().parents[2]
    listing = subprocess.run(["git", "-C", str(root), "ls-files", "*.stl"], capture_output=True, text=True, check=True)
    return [root / name for name in listing.stdout.split()]


def test_a_documented_member_gets_a_blank_line_above_its_comment() -> None:
    """Without it the comment reads as a trailer to the member above rather than the one below."""
    text = "class C\n{\n  var a : u16;\n  // what b is\n  var b : u16;\n}\n"
    assert formatter.format_text(text) == "class C\n{\n  var a : u16;\n\n  // what b is\n  var b : u16;\n}\n"


def test_the_first_member_needs_no_blank_line() -> None:
    """Right after the brace there is nothing above for the comment to be mistaken for."""
    text = "class C\n{\n  // what a is\n  var a : u16;\n}\n"
    assert formatter.format_text(text) == text


def test_a_comment_above_a_declaration_is_left_alone() -> None:
    """Only members are separated; a comment outside a body documents the type itself."""
    text = "// what S is\nstruct S\n{\n  a : u16,\n}\n"
    assert formatter.format_text(text) == text


def test_a_trailing_comment_needs_no_blank_line() -> None:
    """A comment beside its member already reads as belonging to it."""
    text = "class C\n{\n  var a    : u16;     // what a is\n  var bbbb : string;  // what b is\n}\n"
    assert formatter.format_text(text) == text


def test_a_ruler_is_not_a_member_comment() -> None:
    """The scanner drops //-- and // -- as separators, so they document nothing."""
    text = "class C\n{\n  fn a();\n  // -- section --\n  fn b();\n}\n"
    assert formatter.format_text(text) == text


def test_a_ruler_does_not_join_the_comment_below_it() -> None:
    """The blank belongs between the separator and the documentation, as the corpus has it."""
    text = "class C\n{\n  // -- section --\n  // what b does\n  fn b();\n}\n"
    assert formatter.format_text(text) == "class C\n{\n  // -- section --\n\n  // what b does\n  fn b();\n}\n"


def test_a_transclusion_marker_stays_against_what_it_marks() -> None:
    """A blank after the marker would start the documented region with an empty line."""
    text = "class C\n{\n  // --8<-- [start:commands]\n  // clears the screen\n  fn clear();\n}\n"
    assert formatter.format_text(text) == text


def test_a_lone_member_also_gets_one_space_before_its_attributes() -> None:
    """The rule is about the member, not about the run it happens to sit in."""
    text = "class C\n{\n  // what it is\n  var prop5 : i32  [writable];\n}\n"
    assert formatter.format_text(text) == "class C\n{\n  // what it is\n  var prop5 : i32 [writable];\n}\n"


def test_functions_and_events_get_one_space_before_their_attributes() -> None:
    """Neither is a member run, and both carry attributes."""
    text = "class C\n{\n  fn play()  [confirmed];\n\n  event e(id : u32)     [bestEffort];\n}\n"
    assert (
        formatter.format_text(text) == "class C\n{\n  fn play() [confirmed];\n\n  event e(id : u32) [bestEffort];\n}\n"
    )
