# === test_session.py ==================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""Integration tests that drive the term binary through a pseudo terminal.

Every unit test in the suite passes whether or not the binary starts at all, because nothing in the
suite starts it. These do.
"""

import argparse
import sys
import unittest
from pathlib import Path

sys.path.append(str(Path(__file__).parent.parent))
from term_runner import ARROW_RIGHT, CTRL_D, ESCAPE, HOME, PASTE_END, PASTE_START, TAB, TermTester


class TestTermSession(unittest.TestCase):
    """Drives one term session per test."""

    cli_run_path: str = ""
    config_yaml: str = ""

    def setUp(self) -> None:
        """Start a term and wait for its first frame."""
        self.term: TermTester = TermTester(self.cli_run_path, self.config_yaml)
        self.term.start()

    def tearDown(self) -> None:
        """Kill the term if the test left it running."""
        self.term.stop()

    def test_the_binary_starts_and_draws_a_prompt(self) -> None:
        """The kernel loads the term and the term paints a prompt."""
        screen: str = self.term.screen()
        self.assertIn("sen:", screen, f"no prompt painted. Screen:\n{screen}")
        self.assertTrue(self.term.is_running(), "the process died after drawing its first frame")

    def test_help_lists_the_commands(self) -> None:
        """`help` reaches the command table and prints it."""
        self.term.send_command("help")
        self.assertTrue(self.term.wait_for("inspect"), f"help did not list 'inspect'. Screen:\n{self.term.screen()}")
        for command in ("listen", "query", "open"):
            self.assertIn(command, self.term.screen(), f"help did not list '{command}'")

    def test_ls_shows_the_objects_on_the_bus(self) -> None:
        """Navigating to the bus and listing it reaches the object store and the tree renderer."""
        self.term.send_command("cd local.demo")
        self.term.send_command("ls")
        self.assertTrue(
            self.term.wait_for("showcase"), f"'showcase' not listed on local.demo. Screen:\n{self.term.screen()}"
        )

    def test_the_displayed_scope_path_can_be_typed_back(self) -> None:
        """The prompt's own path is a valid target.

        The group path used to be displayed with the separator that also means "session.bus", so
        copying it out of the prompt and typing it back silently moved the user to another session.
        """
        self.term.send_command("cd local.demo")
        self.term.send_command("cd alpha")
        self.assertTrue(
            self.term.wait_for("sen:/local.demo/alpha"),
            f"the nested group is not displayed as a path. Screen:\n{self.term.screen()}",
        )

        # Leave, and forget the frames that already showed the path, so what follows can only be
        # satisfied by the new navigation.
        self.term.send_command("cd /")
        self.term.forget()

        # Navigate using exactly the string the prompt showed, including its leading separator.
        self.term.send_command("cd /local.demo/alpha")
        self.term.send_command("pwd", settle=1.5)
        screen: str = self.term.screen()
        # The whole prompt, not a fragment of it: the old behaviour produced "sen://local.demo/alpha",
        # which contains "/local.demo/alpha" as a substring and would satisfy a looser assertion.
        self.assertIn("sen:/local.demo/alpha", screen, f"the displayed path did not lead back. Screen:\n{screen}")
        self.assertNotIn("sen://", screen, f"the leading separator was taken as part of a name. Screen:\n{screen}")
        self.assertTrue(self.term.is_running())

    def test_an_unknown_command_reports_and_the_term_survives(self) -> None:
        """A typo is reported and does not take the kernel down with it."""
        self.term.send_command("notacommand")
        self.assertTrue(
            self.term.wait_for("not a recognized command"),
            f"no report for an unknown command. Screen:\n{self.term.screen()}",
        )
        self.term.send_command("pwd")
        self.assertTrue(self.term.is_running(), "the term died on an unknown command")

    def test_completion_keeps_the_rest_of_the_line(self) -> None:
        """Completing a token in the middle of a line leaves the rest of the line alone.

        Every cycling path used to replace from the token start to the end of the buffer, so
        completing the first word deleted every argument after it.
        """
        self.term.type_text("local.de KEEPME")
        self.term.send_keys(HOME)
        self.term.send_keys(ARROW_RIGHT * len("local.de"))

        # Forget the frames that already showed the typed line: the term repaints the whole screen,
        # so asserting on accumulated text would pass even if Tab had deleted the tail.
        self.term.forget()
        self.term.send_keys(TAB, settle=1.5)
        screen: str = self.term.screen()
        self.assertIn("local.demo", screen, f"Tab completed nothing, so the test proves nothing. Screen:\n{screen}")
        self.assertIn("KEEPME", screen, f"completion ate the rest of the line. Screen:\n{screen}")

    def test_ctrl_d_exits_only_from_an_empty_line(self) -> None:
        """Ctrl+D on a half-typed line is refused, and taken on an empty one.

        FTXUI has no bracketed paste, so a 0x04 byte inside pasted text arrives as this key: an
        unconditional exit quit the term in the middle of a paste.
        """
        self.term.type_text("half typed")
        self.term.send_keys(CTRL_D)
        self.assertTrue(self.term.is_running(), "Ctrl+D exited with text on the line")
        self.assertTrue(
            self.term.wait_for("empty line", timeout=2.0),
            f"no explanation for the refused Ctrl+D. Screen:\n{self.term.screen()}",
        )

        # Clear the line, then Ctrl+D is the exit.
        self.term.send_keys(b"\x7f" * len("half typed"))
        self.term.send_keys(CTRL_D)
        self.assertTrue(self.term.wait_for_exit(), "Ctrl+D on an empty line did not shut the kernel down")

    def test_a_huge_paste_leaves_the_term_responsive(self) -> None:
        """A paste far longer than the line accepts is refused, and the term keeps working.

        30,000 characters used to leave the term alive but unable to act on any further key: it
        re-wrapped the whole buffer on every frame, and the layout gave the output pane no rows at
        all, so nothing reported and nothing recovered. Only killing it got out.
        """
        self.term.paste("x" * 30000)
        self.assertTrue(self.term.is_running(), "the term died on a large paste")
        self.assertTrue(
            self.term.wait_for("command line is full", timeout=15.0),
            f"the line was not bounded, or said nothing about it. Screen:\n{self.term.screen()[-2000:]}",
        )

        # And it still acts on input: clear the line, then run a command.
        self.term.send_keys(ESCAPE, settle=1.0)
        self.term.forget()
        self.term.send_command("pwd", settle=2.0)
        self.assertTrue(
            self.term.wait_for("sen:/", timeout=15.0),
            f"the term stopped acting on input after a large paste. Screen:\n{self.term.screen()[-2000:]}",
        )
        self.assertTrue(self.term.is_running())

    def test_the_term_asks_the_terminal_for_bracketed_paste(self) -> None:
        """Without the request, the terminal sends pasted text as plain keystrokes."""
        self.assertIn("\x1b[?2004h", self.term.raw(), "the term did not enable bracketed paste")

    def test_a_multi_line_paste_runs_nothing_and_becomes_one_line(self) -> None:
        """Line breaks in pasted text join into spaces instead of submitting the line.

        One paste of three commands used to run all three, synchronously, inside one tick — including
        anything destructive that happened to be in the text.
        """
        self.term.forget()
        self.term.send_keys(PASTE_START + b"help\rhelp\r" + PASTE_END, settle=2.5)
        screen: str = self.term.screen()
        self.assertNotIn("inspect", screen, f"the pasted 'help' was executed. Screen:\n{screen}")
        self.assertIn("help help", screen, f"the two pasted lines were not joined. Screen:\n{screen}")
        self.assertIn("line breaks, joined", screen, f"nothing said the lines had been joined. Screen:\n{screen}")
        self.assertTrue(self.term.is_running())

    def test_a_tab_inside_a_paste_is_text_not_completion(self) -> None:
        """A control character in pasted text does not act as the key of the same name."""
        self.term.forget()
        self.term.send_keys(PASTE_START + b"loc\tal" + PASTE_END, settle=2.0)
        screen: str = self.term.screen()
        self.assertIn("loc al", screen, f"the pasted tab did not become a space. Screen:\n{screen}")
        self.assertNotIn("local.demo", screen, f"the pasted tab opened completion. Screen:\n{screen}")

    def test_a_paste_whose_end_marker_is_lost_releases_itself(self) -> None:
        """A paste that never finishes must not leave the term swallowing every key.

        The terminal flushes an incomplete escape sequence after 50 ms, so a paste that stalls inside
        the six bytes of the end marker — over ssh, or through tmux — delivers it in pieces and none of
        them matches. With nothing to end the paste, every later key was swallowed and no gesture
        recovered it: the user had to kill the process.
        """
        self.term.forget()
        self.term.send_keys(PASTE_START + b"stalled", settle=1.0)
        # The end marker, split by a pause longer than the terminal's flush window.
        self.term.send_keys(b"\x1b[20", settle=0.6)
        self.term.send_keys(b"1~", settle=1.0)

        self.assertTrue(
            self.term.wait_for("did not finish", timeout=15.0),
            f"the paste never released. Screen:\n{self.term.screen()[-1500:]}",
        )

        # And the term acts on keys again.
        self.term.send_keys(ESCAPE, settle=1.0)
        self.term.forget()
        self.term.send_command("pwd", settle=2.0)
        self.assertTrue(
            self.term.wait_for("sen:/", timeout=10.0),
            f"the term was still swallowing input. Screen:\n{self.term.screen()[-1500:]}",
        )

    def test_escape_cancels_a_paste_in_progress(self) -> None:
        """The second way out, so neither depends on the other."""
        self.term.forget()
        self.term.send_keys(PASTE_START + b"half a paste", settle=1.0)
        self.term.send_keys(ESCAPE, settle=1.0)
        self.assertTrue(
            self.term.wait_for("Paste cancelled", timeout=10.0),
            f"Escape did not end the paste. Screen:\n{self.term.screen()[-1500:]}",
        )

        # Keys act again straight away, well inside the idle release.
        self.term.forget()
        self.term.send_command("pwd", settle=2.0)
        self.assertTrue(self.term.wait_for("sen:/", timeout=6.0), "the term did not act on input after cancelling")

    def test_the_line_completion_produces_gets_a_useful_answer(self) -> None:
        """Enter on the exact line Tab leaves behind must not be reported as an unknown command.

        Accepting an object candidate appends a dot, so `local.demo.showcase.` is one keystroke away and
        the user never typed the dot. Two functions read that string differently — completion splits it
        to list the methods, execution refuses it — and the error quoted the string back as though the
        user had invented it.
        """
        self.term.send_command("cd local.demo")
        self.term.forget()

        # Let completion build the line, rather than typing what we think it builds.
        self.term.type_text("showcas")
        self.term.send_keys(TAB, settle=1.5)
        self.term.send_keys(b"\r", settle=2.0)

        screen: str = self.term.screen()
        self.assertNotIn(
            "is not a recognized command", screen, f"the completed line was called unknown. Screen:\n{screen}"
        )
        self.assertIn("names no method", screen, f"no useful answer for the dangling dot. Screen:\n{screen}")
        self.assertTrue(self.term.is_running())

    def test_exit_shuts_the_kernel_down(self) -> None:
        """The `exit` command stops the process rather than just the component."""
        self.term.send_command("exit")
        self.assertTrue(
            self.term.wait_for_exit(), f"the process kept running after 'exit'. Screen:\n{self.term.screen()}"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli_run_path", help="path to the cli_run binary")
    parser.add_argument("config_yaml", help="path to the session configuration")
    parser.add_argument("test_name", help="the unittest to run, e.g. TestTermSession.test_help_lists_the_commands")
    args = parser.parse_args()

    TestTermSession.cli_run_path = args.cli_run_path
    TestTermSession.config_yaml = args.config_yaml
    result = unittest.main(argv=[sys.argv[0], args.test_name], exit=False).result
    sys.exit(0 if result.wasSuccessful() else 1)
