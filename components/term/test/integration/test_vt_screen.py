# === test_vt_screen.py ===============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# =====================================================================================================================
"""Tests for the grid model the session tests assert row numbers against.

The model had no tests of its own, so the two places it differed from a terminal — no auto-wrap and no
scroll — were invisible. Both shift every row number below the offending line, which is the one property
a grid model exists to provide.
"""

from __future__ import annotations

from vt_screen import VtScreen


def test_a_row_wider_than_the_screen_wraps() -> None:
    """A row past the last column continues on the next one."""
    screen = VtScreen(rows=4, columns=5)
    screen.feed("abcdefg")
    assert screen.line(1) == "abcde"
    assert screen.line(2).rstrip() == "fg"


def test_wrapping_does_not_shift_the_rows_below_it() -> None:
    """The line after a wrapped one is where a terminal puts it."""
    # Without auto-wrap the overflow was dropped and the column kept counting, so "second" landed on
    # row 2 and every assertion about a later row was off by one.
    screen = VtScreen(rows=4, columns=5)
    # "next" fits in one row; "abcdefg" does not, so it takes two. Row 3 is therefore where the line
    # after the wrapped one belongs.
    screen.feed("abcdefg\nnext")
    assert screen.line(3).rstrip() == "next"


def test_the_bottom_row_scrolls_instead_of_clamping() -> None:
    """A newline on the last row moves the frame up, as a terminal does."""
    screen = VtScreen(rows=3, columns=8)
    screen.feed("one\ntwo\nthree\nfour")
    # "one" has gone off the top; the rest moved up by one.
    assert screen.line(1).rstrip() == "two"
    assert screen.line(2).rstrip() == "three"
    assert screen.line(3).rstrip() == "four"


def test_carriage_return_overwrites_from_the_start_of_the_row() -> None:
    """CR returns to column one without erasing."""
    screen = VtScreen(rows=2, columns=8)
    screen.feed("abcdef\rXY")
    assert screen.line(1).rstrip() == "XYcdef"


def test_backspace_moves_left_without_erasing() -> None:
    """Backspace moves the cursor only."""
    screen = VtScreen(rows=2, columns=8)
    screen.feed("abc\b\bZ")
    assert screen.line(1).rstrip() == "aZc"


def test_row_of_finds_the_first_matching_row() -> None:
    """row_of answers with the topmost match, and 0 for none."""
    screen = VtScreen(rows=3, columns=12)
    screen.feed("alpha\nbeta\nalpha")
    assert screen.row_of("alpha") == 1
    assert screen.row_of("beta") == 2
    assert screen.row_of("absent") == 0
