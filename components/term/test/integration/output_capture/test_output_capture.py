# === test_output_capture.py ===========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""Every route a program has to the terminal, and whether term shows it or paints over it.

A user reported log lines that appeared for about a tenth of a second and then vanished, with
only term's own entries left when they scrolled back. The routes that failed were the ones nothing
here covered: `std::cout`, `printf`, and a logger made by spdlog's own factory rather than through
the kernel.

These assert on `grid()` rather than `screen()`, and the difference is the whole test. `screen()`
accumulates every byte the term ever painted, so a line that was written to the descriptor and then
overwritten by the next frame is still in it and an assertion on it passes while the user sees
nothing. `grid()` is the frame after the repaints, which is what a user actually has in front of
them.
"""

import argparse
import sys
import time
import unittest
from pathlib import Path

sys.path.append(str(Path(__file__).parent.parent))
from term_runner import TermTester
from vt_screen import VtScreen

# Each emitter in the config tags its lines with one of these, so a failure names the route.
KERNEL_ROUTE = "KERNELROUTE"
STDERR_ROUTE = "STDERRROUTE"
COUT_ROUTE = "COUTROUTE"
PRINTF_ROUTE = "PRINTFROUTE"
PLAIN_ROUTE = "PLAINROUTE"


class TestOutputCapture(unittest.TestCase):
    """Drives one term session per route."""

    cli_run_path: str = ""
    config_yaml: str = ""

    def setUp(self) -> None:
        """Start a term and let the emitters produce a few lines."""
        self.term: TermTester = TermTester(self.cli_run_path, self.config_yaml)
        self.term.start()

    def tearDown(self) -> None:
        """Stop the term after each test."""
        self.term.stop()

    @staticmethod
    def _separator_row(grid: VtScreen) -> int:
        """The rule term draws between its output pane and its input line.

        Everything term renders is above it. Anything a write to the descriptor leaves on screen
        lands wherever the cursor was, which is the prompt, below it.
        """
        for index, line in enumerate(grid.lines()):
            if line.strip().startswith("\u2500" * 8):
                return index
        return -1

    def _wait_until_in_the_pane(self, needle: str, seconds: float = 8.0) -> bool:
        """True once `needle` is inside term's output pane, not merely somewhere on the screen.

        Being on the screen is not the property under test and asserting it passes on the defect:
        an uncaptured write still reaches the terminal, so it appears in the grid, painted across
        term's prompt and gone on the next repaint of those cells. The pane is what scrolls, what
        survives a redraw, and what the user can read.
        """
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            grid = self.term.grid()
            separator = self._separator_row(grid)
            if separator > 0:
                pane = grid.lines()[:separator]
                if any(needle in line for line in pane):
                    return True
            self.term.read_output(timeout=0.4)
        return False

    def _report(self, needle: str) -> str:
        """Says whether the line never arrived or arrived and was painted over.

        Without this a red test cannot be read: an emitter that never ran and a line the renderer
        overwrote both leave the grid without the needle, and they are different defects.
        """
        grid = self.term.grid()
        separator = self._separator_row(grid)
        on_screen_somewhere = any(needle in line for line in grid.lines())
        if on_screen_somewhere:
            return (
                f"{needle} is on the screen but not in the pane above row {separator}, so it was "
                f"written straight to the descriptor and term never captured it.\n"
                f"Grid:\n{grid.numbered()}"
            )
        if needle in self.term.raw():
            return (
                f"{needle} reached the pty and is not in the frame at all, so it was painted "
                f"over.\nGrid:\n{grid.numbered()}"
            )
        return (
            f"{needle} never reached the pty at all, so the emitter did not run and this test "
            f"says nothing about capture.\nGrid:\n{self.term.grid().numbered()}"
        )

    def test_a_kernel_logger_line_is_visible(self) -> None:
        """The control. A logger the kernel made was visible before this change and after it.

        If this fails the harness is broken and the other four results mean nothing.
        """
        self.assertTrue(self._wait_until_in_the_pane(KERNEL_ROUTE), self._report(KERNEL_ROUTE))

    def test_a_stderr_line_is_visible(self) -> None:
        """Standard error was never the broken half: OutputCapture puts fd 2 in the same pane."""
        self.assertTrue(self._wait_until_in_the_pane(STDERR_ROUTE), self._report(STDERR_ROUTE))

    def test_a_cout_line_is_visible(self) -> None:
        """`std::cout` shares fd 1 with the renderer, which is the reported defect."""
        self.assertTrue(self._wait_until_in_the_pane(COUT_ROUTE), self._report(COUT_ROUTE))

    def test_a_printf_line_is_visible(self) -> None:
        """`printf` reaches fd 1 through the C stdout FILE*, which is a spdlog console sink's route."""
        self.assertTrue(self._wait_until_in_the_pane(PRINTF_ROUTE), self._report(PRINTF_ROUTE))

    def test_a_plain_spdlog_line_is_visible(self) -> None:
        """A logger from spdlog's own factory, which no kernel route reaches. ESCA's case."""
        self.assertTrue(self._wait_until_in_the_pane(PLAIN_ROUTE), self._report(PLAIN_ROUTE))


def main() -> None:
    """Main execution entry point."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description="Run the term output-capture tests.")
    parser.add_argument("cli_run_path", help="Path to the cli_run executable")
    parser.add_argument("config_yaml", help="Path to the yaml config file")
    parser.add_argument("unittest_args", nargs="*", help="Specific unit test method to run")

    args: argparse.Namespace = parser.parse_args()

    TestOutputCapture.cli_run_path = args.cli_run_path
    TestOutputCapture.config_yaml = args.config_yaml

    sys.argv = [sys.argv[0]] + args.unittest_args
    unittest.main()


if __name__ == "__main__":
    main()
