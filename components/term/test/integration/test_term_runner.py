# === test_term_runner.py ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# =====================================================================================================================
"""Tests for the reader the session tests build their screens from.

What the harness reads is the evidence every session test argues from, so a fault here reads as a fault
in the term. This covers the one that did: a character split across two reads.
"""

from __future__ import annotations

import os
import sys

import pytest

if sys.platform == "win32":  # pragma: no cover - the harness needs pty, fcntl and termios
    pytest.skip("the harness is POSIX-only, as is the suite it drives", allow_module_level=True)

from term_runner import TermTester


def test_a_character_split_across_two_reads_survives() -> None:
    """A read boundary inside a multi-byte character leaves the character intact."""
    # The separator row the term draws is made of these, and one mangled into replacement characters
    # is three columns where there was one, which is enough to wrap the row and scroll the grid.
    glyph = "─".encode()
    reader, writer = os.pipe()
    tester = TermTester("", "")
    tester.fd = reader
    try:
        os.write(writer, b"a" + glyph[:1])
        tester.read_output(timeout=0.3)
        os.write(writer, glyph[1:] + b"b")
        tester.read_output(timeout=0.3)
    finally:
        os.close(writer)
        os.close(reader)

    assert tester.raw() == "a─b"
