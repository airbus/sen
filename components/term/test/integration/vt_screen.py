# === vt_screen.py =====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""A terminal screen, reconstructed from what the term painted.

Stripping the escape sequences tells you *what* text was painted but not *where*: the term repaints
the whole screen, so the accumulated text holds every frame at once and says nothing about the
current layout. Replaying the cursor movements into a grid answers "which row is this on", which is
the only way to assert on placement.

Only the sequences the term actually emits are interpreted (cursor positioning, erase, and the
relative moves); colour and mode sequences are consumed and discarded.
"""

import re

_CSI = re.compile(r"\x1b\[([0-9;?]*)([A-Za-z])")
_SHORT_ESCAPE = re.compile(r"\x1b[()#][0-9A-Za-z]|\x1b[=>78MDEc]")
_TAB_WIDTH = 8


class VtScreen:
    """A character grid that text is painted into, with no scrollback.

    The term runs in the alternate screen, which has no scrollback either, so a grid of the pty's
    size holds exactly what a user would see.
    """

    def __init__(self, rows: int, columns: int) -> None:
        """Start with a blank grid of the pty's geometry and the cursor at the top left."""
        self.rows: int = rows
        self.columns: int = columns
        self._grid: list[list[str]] = [[" "] * columns for _ in range(rows)]
        self._row: int = 0
        self._column: int = 0

    def feed(self, data: str) -> None:
        """Replay painted output into the grid."""
        index = 0
        while index < len(data):
            character = data[index]
            if character == "\x1b":
                index += self._escape(data, index)
                continue
            self._character(character)
            index += 1

    def line(self, row: int) -> str:
        """One row, 1-based to match what a person counting rows on screen would say."""
        return "".join(self._grid[row - 1]).rstrip()

    def lines(self) -> list[str]:
        """Every row, top to bottom, trailing blanks removed from each."""
        return [self.line(row) for row in range(1, self.rows + 1)]

    def numbered(self) -> str:
        """The whole grid with row numbers, for a failure message."""
        return "\n".join(f"{row:3d}|{self.line(row)}" for row in range(1, self.rows + 1))

    def row_of(self, needle: str) -> int:
        """The 1-based row holding `needle`, or 0 when no row does."""
        for row in range(1, self.rows + 1):
            if needle in self.line(row):
                return row
        return 0

    def _scroll(self) -> None:
        """Drop the top row and add a blank one, the way a terminal does at the bottom."""
        self._grid.pop(0)
        self._grid.append([" "] * self.columns)

    def _newline(self) -> None:
        if self._row + 1 >= self.rows:
            self._scroll()
        else:
            self._row += 1
        self._column = 0

    def _character(self, character: str) -> None:
        """Paint one character, or act on one of the control codes the term emits."""
        if character == "\n":
            self._newline()
        elif character == "\r":
            self._column = 0
        elif character == "\b":
            self._column = max(0, self._column - 1)
        elif character == "\t":
            self._column = min(self.columns - 1, (self._column // _TAB_WIDTH + 1) * _TAB_WIDTH)
        elif character >= " ":
            # Auto-wrap, because a real terminal has DECAWM on. Dropping the overflow and carrying on
            # incrementing the column put every row below an over-wide one at the wrong number, which is
            # the one thing a grid model exists to get right.
            if self._column >= self.columns:
                self._newline()
            self._grid[self._row][self._column] = character
            self._column += 1

    def _escape(self, data: str, index: int) -> int:
        """Act on one escape sequence at `index` and return how many characters it took."""
        match = _CSI.match(data, index)
        if match is None:
            short = _SHORT_ESCAPE.match(data, index)
            return len(short.group(0)) if short is not None else 1

        numbers = [int(part) for part in match.group(1).split(";") if part.isdigit()]
        final = match.group(2)
        first = numbers[0] if numbers else 0
        step = numbers[0] if numbers else 1

        if final in "Hf":
            self._row = (numbers[0] - 1) if len(numbers) > 0 else 0
            self._column = (numbers[1] - 1) if len(numbers) > 1 else 0
        elif final == "J":
            self._erase_display(first)
        elif final == "K":
            self._erase_line(first)
        elif final == "A":
            self._row = max(0, self._row - step)
        elif final == "B":
            self._row = min(self.rows - 1, self._row + step)
        elif final == "C":
            self._column = min(self.columns - 1, self._column + step)
        elif final == "D":
            self._column = max(0, self._column - step)
        return len(match.group(0))

    def _erase_display(self, mode: int) -> None:
        """ED: 2 clears the screen, 0 clears from the cursor down."""
        if mode == 2:
            self._grid = [[" "] * self.columns for _ in range(self.rows)]
        elif mode == 0:
            self._grid[self._row][self._column :] = [" "] * (self.columns - self._column)
            for row in range(self._row + 1, self.rows):
                self._grid[row] = [" "] * self.columns

    def _erase_line(self, mode: int) -> None:
        """EL: 0 clears from the cursor to the end of the row, 2 clears the row."""
        if mode == 0:
            self._grid[self._row][self._column :] = [" "] * (self.columns - self._column)
        elif mode == 2:
            self._grid[self._row] = [" "] * self.columns
