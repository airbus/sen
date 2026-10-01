# === test_ls_sources.py ===============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""What `ls` lists besides objects: the sessions discovery has seen and the buses that are open.

Ported from the shell's `ls_command` suite, case for case, because users moving from the shell expect
the same screen. The shell builds its tree from the sessions discovery reports and decorates each node,
so an open bus holding nothing and a session nobody has opened both appear.
"""

import argparse
import sys
import unittest
from pathlib import Path

sys.path.append(str(Path(__file__).parent.parent))
from term_runner import TermTester


class TestLsSources(unittest.TestCase):
    """Drives one term session per test."""

    cli_run_path: str = ""
    config_yaml: str = ""

    def setUp(self) -> None:
        """Start a term before each test."""
        self.term: TermTester = TermTester(self.cli_run_path, self.config_yaml)
        self.term.start()

    def tearDown(self) -> None:
        """Stop the term after each test."""
        self.term.stop()

    def _listing(self, after: str = "") -> str:
        """Returns a screen holding only an `ls` listing.

        `open` leaves a confirmation in the output pane and the typed line stays on screen, and either
        satisfies a substring assertion without `ls` having listed anything. Clearing the output and
        forgetting the frames first leaves the listing as the only thing that can satisfy it.
        """
        if after:
            self.term.send_command(after)
        self.term.send_command("clear")
        self.term.forget()
        self.term.send_command("ls")
        return self.term.screen()

    def test_an_unopened_session_is_listed(self) -> None:
        """A session discovery has seen is listed before anyone opens it, tagged as not open."""
        listing = self._listing()
        self.assertRegex(listing, r"other_session\s*\[~\]", f"Screen:\n{listing}")

    def test_an_open_bus_with_no_objects_is_listed(self) -> None:
        """A bus that is open and empty.

        `local.quiet` is opened by the configuration and nobody publishes on it, so no object can put it
        in the tree and discovery never detects it. It exists only in the open set.
        """
        self.assertRegex(self._listing(), r"quiet\s*\[bus\]")

    def test_opening_a_bus_tags_it_open(self) -> None:
        """Opening a discovered bus moves it from not-open to open in the listing."""
        self.assertRegex(self._listing("open other_session.other_bus"), r"other_bus\s*\[bus\]")

    def test_an_open_session_with_no_objects_is_listed(self) -> None:
        """A session that is open and empty.

        Nobody publishes on it, so no object can put it in the tree, and `getAvailableSources` reports
        an open session only through its buses, of which it has none.
        """
        self.assertRegex(self._listing("open empty_session"), r"empty_session\s*\[session\]")


def main() -> None:
    """Main execution entry point."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description="Run the term ls-sources tests.")
    parser.add_argument("cli_run_path", help="Path to the cli_run executable")
    parser.add_argument("config_yaml", help="Path to the yaml config file")
    parser.add_argument("unittest_args", nargs="*", help="Specific unit test method to run")

    args: argparse.Namespace = parser.parse_args()

    TestLsSources.cli_run_path = args.cli_run_path
    TestLsSources.config_yaml = args.config_yaml

    sys.argv = [sys.argv[0]] + args.unittest_args
    unittest.main()


if __name__ == "__main__":
    main()
