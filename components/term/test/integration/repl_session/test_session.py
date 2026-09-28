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
from term_runner import (
    ARROW_RIGHT,
    ARROW_UP,
    CTRL_D,
    CTRL_U,
    ESCAPE,
    F1,
    HOME,
    PAGE_DOWN,
    PAGE_UP,
    PASTE_END,
    PASTE_START,
    TAB,
    TermTester,
)


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

        Displaying the group path with the separator that also means "session.bus" would let a user copy it
        out of the prompt, type it back, and be moved silently to another session.
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
        # The whole prompt, not a fragment of it: a doubled separator gives "sen://local.demo/alpha", which
        # contains "/local.demo/alpha" as a substring and would satisfy a looser assertion.
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

        A cycling path that replaces from the token start to the end of the buffer would delete every
        argument after the word being completed.
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

        Unbounded, 30,000 characters leave the term alive but unable to act on a key: it re-wraps the whole
        buffer every frame, and the layout gives the output pane no rows, so nothing is reported and nothing
        recovers short of killing it.
        """
        self.term.paste("x" * 30000)
        self.assertTrue(self.term.is_running(), "the term died on a large paste")
        self.assertTrue(
            self.term.wait_for("command line is full", timeout=15.0),
            f"the line was not bounded, or said nothing about it. Screen:\n{self.term.screen()[-2000:]}",
        )

        # And it still acts on input. The drain first is not politeness: 30,000 characters are still
        # coming out of the pty, and a probe sent now queues behind them, so Escape clears a line that
        # the remaining characters immediately refill. The old probe could not tell that apart from a
        # term that had stopped reading keys, because it waited for the prompt, which every idle repaint
        # draws.
        self.term.read_output(timeout=8.0)
        self.term.send_keys(ESCAPE, settle=1.0)
        self.term.forget()
        self.term.send_command("xyzzy", settle=2.0)
        self.assertTrue(
            self.term.wait_for("Nothing happens.", timeout=15.0),
            f"the term stopped acting on input after a large paste. Screen:\n{self.term.screen()[-2000:]}",
        )
        self.assertTrue(self.term.is_running())

    def test_the_term_asks_the_terminal_for_bracketed_paste(self) -> None:
        """Without the request, the terminal sends pasted text as plain keystrokes."""
        self.assertIn("\x1b[?2004h", self.term.raw(), "the term did not enable bracketed paste")

    def test_a_multi_line_paste_runs_nothing_and_becomes_one_line(self) -> None:
        """Line breaks in pasted text join into spaces instead of submitting the line.

        Without the paste state, one paste of three commands runs all three inside a single tick, including
        anything destructive that happens to be in the text.
        """
        self.term.forget()
        self.term.send_keys(PASTE_START + b"help\rhelp\r" + PASTE_END, settle=2.5)
        screen: str = self.term.screen()
        self.assertNotIn("inspect", screen, f"the pasted 'help' was executed. Screen:\n{screen}")
        self.assertIn("help help", screen, f"the two pasted lines were not joined. Screen:\n{screen}")
        self.assertIn("line breaks, joined", screen, f"nothing said the lines had been joined. Screen:\n{screen}")
        self.assertTrue(self.term.is_running())

    def test_a_tab_inside_a_paste_is_text_not_completion(self) -> None:
        """A control character in pasted text does not act as the key of the same name.

        Asserted on the prompt row alone. An earlier version looked for "local.demo" anywhere on the
        screen, which stopped meaning anything once log messages began rendering into the output area:
        the bus name appears there for its own reasons, and the test failed on a working term.
        """
        self.term.forget()
        self.term.send_keys(PASTE_START + b"loc\tal" + PASTE_END, settle=2.0)

        prompt_rows = [row for row in self.term.screen().split("\n") if "sen:" in row]
        self.assertTrue(prompt_rows, f"no prompt row on screen. Screen:\n{self.term.screen()}")
        prompt = prompt_rows[-1]

        self.assertIn("loc al", prompt, f"the pasted tab did not become a space. Prompt: {prompt!r}")
        self.assertNotIn("local.", prompt, f"the pasted tab completed the token. Prompt: {prompt!r}")

    def test_a_paste_whose_end_marker_never_arrives_releases_itself(self) -> None:
        """A paste that never finishes must not leave the term swallowing every key.

        This is the safety net, so it sends no end marker at all: nothing tells the term the paste is
        over, and only the idle limit can release it. Without that limit every later key is swallowed
        and no gesture recovers it, leaving the user nothing to do but kill the process.

        The separate case below covers a marker that arrives torn, which is the common cause and is now
        handled without waiting for this net.
        """
        self.term.forget()
        self.term.send_keys(PASTE_START + b"stalled", settle=1.0)

        self.assertTrue(
            self.term.wait_for("did not finish", timeout=15.0),
            f"the paste never released. Screen:\n{self.term.screen()[-1500:]}",
        )

        # And the term acts on keys again.
        self.term.send_keys(ESCAPE, settle=1.0)
        self.term.forget()
        self.term.send_command("xyzzy", settle=2.0)
        self.assertTrue(
            self.term.wait_for("Nothing happens.", timeout=10.0),
            f"the term was still swallowing input. Screen:\n{self.term.screen()[-1500:]}",
        )

    def test_a_torn_end_marker_ends_the_paste_and_leaves_no_stray_bytes(self) -> None:
        """A marker split by a slow link must not leave its tail on the command line.

        The terminal flushes an incomplete escape after 50 ms and each marker is six bytes, so over ssh
        or through tmux the end marker arrives in pieces. The prefix matched nothing and was swallowed;
        the remaining bytes arrived as ordinary characters and were appended to the line. The user's
        pasted command came out with a stray "1~" glued on, and only the two-second idle limit ended the
        paste.
        """
        self.term.forget()
        self.term.send_keys(PASTE_START + b"pwd", settle=1.0)
        # The end marker, split by a pause longer than the terminal's flush window.
        self.term.send_keys(b"\x1b[20", settle=0.6)
        self.term.send_keys(b"1~", settle=1.0)

        screen = self.term.screen()
        self.assertNotIn("pwd1~", screen, f"the marker's tail was appended to the line:\n{screen[-1500:]}")
        self.assertNotIn(
            "did not finish",
            screen,
            f"the torn marker should end the paste, not wait for the idle limit:\n{screen[-1500:]}",
        )

        # The line is intact and the term is out of paste mode: Enter runs what was pasted.
        self.term.forget()
        self.term.send_keys(b"\r", settle=1.5)
        self.assertTrue(
            self.term.wait_for("sen:/", timeout=10.0),
            f"the pasted line did not run. Screen:\n{self.term.screen()[-1500:]}",
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

        # Escape kept the text received so far, which is the point of it, so the line has to be cleared
        # before a probe can be a probe: "half a pastexyzzy" is not a command and answers nothing.
        self.term.send_keys(ESCAPE, settle=0.6)
        self.term.forget()
        self.term.send_command("xyzzy", settle=2.0)
        self.assertTrue(
            self.term.wait_for("Nothing happens.", timeout=6.0), "the term did not act on input after cancelling"
        )

    def test_the_line_completion_produces_gets_a_useful_answer(self) -> None:
        """Enter on the exact line Tab leaves behind must not be reported as an unknown command.

        Accepting an object candidate appends a dot, so `local.demo.showcase.` is one keystroke away and
        the user never typed the dot. Two functions read that string differently: completion splits it to
        list the methods, execution refuses it. The error must not quote the string back as though the
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

    def test_output_fills_the_pane_from_the_top(self) -> None:
        """Short output sits at the top of the pane, not pushed down against the prompt.

        The premise is established rather than assumed. `clear` empties the pane, so whatever appears
        next is this test's own output and not kernel log traffic that happened to arrive. Asserting
        that row 1 is non-empty after a `cd`, which prints nothing, would pass on log lines alone and
        would pass with the property broken.
        """
        self.term.send_command("clear", settle=1.0)
        self.term.forget()
        self.term.send_command("pwd", settle=1.5)

        screen = self.term.grid()
        row = screen.row_of("pwd")
        self.assertNotEqual(0, row, f"the command echo never appeared:\n{screen.numbered()}")
        self.assertLessEqual(row, 3, f"short output was pushed down the pane:\n{screen.numbered()}")

        rule_row: int = self.term.rows - 1
        self.assertEqual(
            "",
            screen.line(rule_row - 1).strip(),
            f"the row above the rule is used, so output hugs the prompt:\n{screen.numbered()}",
        )

    def test_the_banner_is_drawn_at_the_top_of_the_screen(self) -> None:
        """With the logo on, the banner sits at the top with one blank row above it."""
        self.term.stop()  # one kernel at a time: this case needs a session the logo is on for
        logo_config: str = str(Path(self.config_yaml).with_name("session_logo.yaml"))
        banner_term: TermTester = TermTester(self.cli_run_path, logo_config)
        self.addCleanup(banner_term.stop)
        banner_term.start()
        banner_term.wait_for("Sen v")
        screen = banner_term.grid()
        self.assertEqual(2, screen.row_of("Sen v"), f"the banner is not on row 2:\n{screen.numbered()}")
        self.assertEqual(
            "", screen.line(1).strip(), f"row 1 should be the blank row above the banner:\n{screen.numbered()}"
        )

    # ---------------------------------------------------------------------------------------------
    # Commands. A complete command table says nothing about whether the handlers behind it run, so
    # each one is executed here and required to answer.
    # ---------------------------------------------------------------------------------------------

    def _run_and_expect(self, command: str, needle: str, settle: float = 1.5) -> None:
        """Run one command and require a recognisable answer, not merely the absence of a crash."""
        self.term.forget()
        self.term.send_command(command, settle=settle)
        screen = self.term.screen()
        self.assertNotIn("is not a recognized command", screen, f"'{command}' was not recognised:\n{screen[-800:]}")
        self.assertIn(needle, screen, f"'{command}' did not answer as expected:\n{screen[-1200:]}")

    def test_status_reports_the_running_components(self) -> None:
        """`status` reaches the kernel's component table."""
        self._run_and_expect("status", "showcaseComponent")

    def test_open_and_close_report_both_ways(self) -> None:
        """Both directions of a source, and both say what they did."""
        self._run_and_expect("close local.demo", "closed")
        self._run_and_expect("open local.demo", "opened")

    def test_query_create_list_and_remove(self) -> None:
        """A named query's whole life, through the route the docs now name."""
        self._run_and_expect("query probes SELECT * FROM local.demo", "probes")
        self._run_and_expect("queries", "probes")
        self._run_and_expect("query rm probes", "removed")

    def test_listen_listeners_and_unlisten(self) -> None:
        """A listener's whole life, on an event the showcase really emits."""
        self._run_and_expect("listen showcase.tick", "showcase.tick")
        self._run_and_expect("listeners", "showcase.tick")
        self._run_and_expect("unlisten showcase", "showcase")

    def test_log_reports_and_sets_a_level(self) -> None:
        """`log` reads the level and `log level` sets it, then puts it back."""
        self._run_and_expect("log", "level")
        self._run_and_expect("log level warn", "warn")
        # Back to info, or later assertions in this process see nothing.
        self._run_and_expect("log level info", "info")

    def test_inspect_shows_an_object_and_a_type(self) -> None:
        """Both kinds of target `inspect` takes."""
        self._run_and_expect("inspect local.demo.showcase", "moveTo")
        self._run_and_expect("inspect term_showcase.Point", "x")

    def test_types_and_units_take_a_filter(self) -> None:
        """The filters match anywhere in the name, which is what help now says."""
        self._run_and_expect("types Showcase", "Showcase")
        self._run_and_expect("units", "velocity")

    def test_theme_switches_and_reports(self) -> None:
        """`theme` answers at all: 311 lines and ten themes had no test."""
        self._run_and_expect("theme", "theme")

    def test_clear_empties_the_output(self) -> None:
        """`clear` empties everything, which is what its help now says."""
        self.term.send_command("help", settle=1.5)
        self.assertTrue(self.term.wait_for("inspect", timeout=8.0))
        self.term.send_command("clear", settle=1.5)
        self.term.forget()
        self.term.read_output(timeout=1.0)
        screen = self.term.grid()
        self.assertEqual(0, screen.row_of("inspect"), f"clear left the help on screen:\n{screen.numbered()}")

    def test_version_reports_the_build(self) -> None:
        """`version` carries the branch the banner does not."""
        self._run_and_expect("version", "branch")

    # ---------------------------------------------------------------------------------------------
    # The guided form, from keystrokes to a submitted call. Nothing tested this path at any level:
    # arg_form_test drives the model directly and never goes through a key.
    # ---------------------------------------------------------------------------------------------

    def test_a_form_opens_takes_values_and_submits_the_call(self) -> None:
        """MoveTo takes a Point, so Enter with no arguments opens a form with x and y."""
        self.term.forget()
        self.term.send_command("cd local.demo", settle=1.5)
        self.term.forget()
        self.term.send_command("showcase.moveTo", settle=2.5)
        self.assertTrue(
            self.term.wait_for("target", timeout=8.0),
            f"the form did not open:\n{self.term.screen()[-1500:]}",
        )
        screen = self.term.screen()
        self.assertIn("x", screen, f"the form has no x field:\n{screen[-1200:]}")

        # Clear the focused field, type a value, move to the next, type again, submit.
        self.term.send_keys(CTRL_U, settle=0.4)
        self.term.type_text("12", settle=0.4)
        self.term.send_keys(TAB, settle=0.4)
        self.term.send_keys(CTRL_U, settle=0.4)
        self.term.type_text("34", settle=0.4)
        self.term.forget()
        self.term.send_keys(b"\r", settle=2.5)

        answer = self.term.screen()
        self.assertNotIn("value required", answer, f"the form refused its own values:\n{answer[-1200:]}")
        self.assertTrue(
            self.term.wait_for("moveTo", timeout=8.0),
            f"the call was never echoed:\n{self.term.screen()[-1200:]}",
        )

    def test_escape_closes_a_form_without_calling(self) -> None:
        """Escape leaves a form and the term keeps answering."""
        self.term.forget()
        self.term.send_command("cd local.demo", settle=1.5)
        self.term.forget()
        self.term.send_command("showcase.moveTo", settle=2.5)
        self.assertTrue(self.term.wait_for("target", timeout=8.0))
        self.term.send_keys(ESCAPE, settle=1.0)
        self.term.forget()
        self.term.send_command("xyzzy", settle=1.5)
        self.assertTrue(
            self.term.wait_for("Nothing happens.", timeout=8.0),
            f"the term did not return to the prompt after Escape:\n{self.term.screen()[-1200:]}",
        )

    # ---------------------------------------------------------------------------------------------
    # Key bindings that were reachable and untested.
    # ---------------------------------------------------------------------------------------------

    def test_f1_runs_help(self) -> None:
        """F1 runs help, which was reachable and documented nowhere."""
        self.term.forget()
        self.term.send_keys(F1, settle=2.0)
        self.assertTrue(
            self.term.wait_for("inspect", timeout=8.0), f"F1 did not run help:\n{self.term.screen()[-800:]}"
        )

    def test_page_up_scrolls_back_and_page_down_returns(self) -> None:
        """Scrolling back leaves the newest line, and scrolling forward comes back to it.

        `units` is hundreds of rows, so there is somewhere to scroll to, and the newest line is the
        marker.
        """
        self.term.send_command("units", settle=3.0)
        self.term.forget()
        self.term.send_command("xyzzy", settle=2.0)
        self.assertTrue(self.term.wait_for("Nothing happens.", timeout=8.0))

        def frame_after(keys: bytes, repeats: int):
            """Forget first, then press, so the grid holds frames the keypresses caused.

            The prompt is checked on every frame: an absence assertion against an empty grid passes for
            the wrong reason, and forgetting *after* the keys leaves exactly that empty grid.
            """
            self.term.forget()
            for _ in range(repeats):
                self.term.send_keys(keys, settle=0.4)
            # One PageDown at the end, always. A frame is only painted when something asks for one, and
            # waiting for the idle repaint is a race that loses under the load of the whole suite.
            # PageDown at the bottom changes nothing and guarantees a redraw.
            self.term.send_keys(PAGE_DOWN if repeats == 0 else keys, settle=0.6)
            self.term.read_output(timeout=1.5)
            grid = self.term.grid()
            self.assertNotEqual(0, grid.row_of("sen:"), f"no frame was drawn at all:\n{grid.numbered()}")
            return grid

        start = frame_after(b"", 0)
        self.assertNotEqual(
            0, start.row_of("Nothing happens."), f"the marker was not in view to start:\n{start.numbered()}"
        )

        scrolled = frame_after(PAGE_UP, 6)
        self.assertEqual(
            0,
            scrolled.row_of("Nothing happens."),
            f"PageUp did not move the view off the newest line:\n{scrolled.numbered()}",
        )

        returned = frame_after(PAGE_DOWN, 12)
        self.assertNotEqual(
            0,
            returned.row_of("Nothing happens."),
            f"PageDown did not come back to the bottom:\n{returned.numbered()}",
        )

    def test_arrow_up_recalls_the_previous_command(self) -> None:
        """History recall, which had no test at any level."""
        self.term.send_command("pwd", settle=1.5)
        self.term.forget()
        self.term.send_keys(ARROW_UP, settle=1.0)
        screen = self.term.grid()
        prompt_row = screen.row_of("pwd")
        self.assertNotEqual(0, prompt_row, f"the previous command was not recalled:\n{screen.numbered()}")

    def test_escape_twice_on_an_empty_line_shuts_the_kernel_down(self) -> None:
        """Two Escapes on an empty line stop the kernel.

        This is the destructive branch. The other Escape test always has text on the line, which takes
        the branch that clears instead.
        """
        self.term.forget()
        self.term.send_keys(ESCAPE, settle=1.0)
        self.term.send_keys(ESCAPE, settle=1.0)
        self.assertTrue(
            self.term.wait_for_exit(timeout=12.0),
            f"two Escapes on an empty line did not stop the kernel. Screen:\n{self.term.screen()[-1200:]}",
        )

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
