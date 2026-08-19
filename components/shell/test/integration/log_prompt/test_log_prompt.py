# === test_log_prompt.py ===============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""Integration tests for the shell prompt behavior with logs."""

import argparse
import subprocess
import sys
import unittest
from pathlib import Path

sys.path.append(str(Path(__file__).parent.parent))
from shell_runner import ShellTester


class TestLogPrompt(unittest.TestCase):
    """Test cases for checking shell prompt behavior with logs."""

    cli_run_path: str = ""
    config_1_yaml: str = ""
    config_2_yaml: str = ""

    def setUp(self) -> None:
        """Set up the shell tester before each test."""
        self.shell: ShellTester = ShellTester(self.cli_run_path, self.config_1_yaml)
        self.initial_output: str = self.shell.start()

    def tearDown(self) -> None:
        """Stop the shell tester after each test."""
        self.shell.stop()

    def test_initial_startup_prompt(self) -> None:
        """Checks that after the initial startup logs, the last line is the shell prompt."""
        out: str = self.initial_output
        lines: list[str] = [line.strip() for line in out.splitlines() if line.strip()]

        self.assertTrue(len(lines) > 0, "No output received during startup.")

        last_line: str = lines[-1]
        self.assertTrue(
            last_line.startswith("sen:") and ">" in last_line,
            f"The last line is not the prompt. Last line: '{last_line}'\nOutput:\n{out}",
        )

    def test_prompt_after_async_log(self) -> None:
        """Checks that the shell prompt is restored automatically after receiving an async log."""
        out1: str = self.initial_output
        lines1: list[str] = [line.strip() for line in out1.splitlines() if line.strip()]

        self.assertTrue(len(lines1) > 0, "No initial output received.")
        self.assertTrue(lines1[-1].startswith("sen:") and ">" in lines1[-1], f"Initial prompt missing. Output:\n{out1}")

        proc2: subprocess.Popen[bytes] = subprocess.Popen(
            [self.cli_run_path, self.config_2_yaml], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )

        try:
            out2: str = self.shell.read_output()
            lines2: list[str] = [line.strip() for line in out2.splitlines() if line.strip()]

            self.assertTrue(len(lines2) > 0, "No log output received.")

            target_log: str = "discovery hub accepted client"
            self.assertIn(target_log, out2, f"Connection log not found in shell. Output:\n{out2}")

            last_line: str = lines2[-1]
            self.assertTrue(
                last_line.startswith("sen:") and ">" in last_line,
                f"Prompt not restored after async log. Last line: '{last_line}'\nOutput:\n{out2}",
            )

        finally:
            proc2.terminate()
            proc2.wait()


def main() -> None:
    """Main execution entry point."""
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description="Run shell integration tests.")
    parser.add_argument("cli_run_path", help="Path to the cli_run executable")
    parser.add_argument("config_1_yaml", help="Path to the main shell yaml config file")
    parser.add_argument("config_2_yaml", help="Path to the trigger yaml config file")
    parser.add_argument("unittest_args", nargs="*", help="Specific unit test method to run")

    args: argparse.Namespace = parser.parse_args()

    TestLogPrompt.cli_run_path = args.cli_run_path
    TestLogPrompt.config_1_yaml = args.config_1_yaml
    TestLogPrompt.config_2_yaml = args.config_2_yaml

    sys.argv = [sys.argv[0]] + args.unittest_args
    unittest.main()


if __name__ == "__main__":
    main()
