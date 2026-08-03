# === test_ls.py =======================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""Integration tests for the shell 'ls' command."""

import argparse
import sys
import unittest
from pathlib import Path

sys.path.append(str(Path(__file__).parent.parent))
from shell_runner import ShellTester


class TestLsCommand(unittest.TestCase):
    """Test cases for the shell 'ls' command."""

    cli_run_path: str = ""
    config_yaml: str = ""

    def setUp(self) -> None:
        """Set up the shell tester before each test."""
        self.shell: ShellTester = ShellTester(self.cli_run_path, self.config_yaml)
        self.shell.start()

    def tearDown(self) -> None:
        """Stop the shell tester after each test."""
        self.shell.stop()

    def test_initial_discovery(self) -> None:
        """Checks that the initial 'ls' command discovers unopened sessions."""
        out: str = self.shell.send_command("ls")
        self.assertIn("other_session [~]", out, f"'other_session [~]' not found. Output:\n{out}")

    def test_open_session(self) -> None:
        """Checks that 'ls' shows the correct tags after opening a session."""
        self.shell.send_command("open other_session")
        out: str = self.shell.send_command("ls")
        self.assertIn(
            "other_session [session]", out, f"'other_session [session]' not found after opening. Output:\n{out}"
        )

    def test_open_bus(self) -> None:
        """Checks that 'ls' shows the correct tags after opening a specific bus."""
        self.shell.send_command("open other_session.other_bus")
        out: str = self.shell.send_command("ls")
        self.assertIn("other_session [session]", out, f"Session tag missing. Output:\n{out}")
        self.assertIn("other_bus [bus]", out, f"Bus tag missing. Output:\n{out}")

    def test_open_empty_session(self) -> None:
        """Checks that 'ls' shows the correct tags when opening an empty session context."""
        self.shell.send_command("open empty_session")
        out: str = self.shell.send_command("ls")
        self.assertIn("empty_session [session]", out, f"'empty_session [session]' not found. Output:\n{out}")


def main() -> None:
    """Main execution entry point."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description="Run shell integration tests.")
    parser.add_argument("cli_run_path", help="Path to the cli_run executable")
    parser.add_argument("config_yaml", help="Path to the yaml config file")
    parser.add_argument("unittest_args", nargs="*", help="Specific unit test method to run")

    args: argparse.Namespace = parser.parse_args()

    TestLsCommand.cli_run_path = args.cli_run_path
    TestLsCommand.config_yaml = args.config_yaml

    sys.argv = [sys.argv[0]] + args.unittest_args
    unittest.main()


if __name__ == "__main__":
    main()
