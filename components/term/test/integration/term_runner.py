# === term_runner.py ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""Drives the term component through a pseudo terminal.

The term draws a full screen rather than printing lines, so `screen()` returns everything read so far
with the escape sequences stripped. Assert on substrings of that: a repaint rewrites text that is
already there, so counting occurrences says nothing about whether something is new.

`grid()` answers the other kind of question. It replays the painted output into a character grid, so
a test can ask which row something is on -- which `screen()` cannot, because it holds every frame at
once.
"""

import fcntl
import os
import pty
import re
import select
import signal
import struct
import termios
import time

from vt_screen import VtScreen

# Control bytes the term binds. FTXUI has no bracketed paste, so these are indistinguishable from
# the same bytes arriving in pasted text -- which is why the term treats Ctrl+D as an exit only on an
# empty line.
CTRL_D = b"\x04"
TAB = b"\t"
ESCAPE = b"\x1b"
CTRL_W = b"\x17"

PASTE_START = b"\x1b[200~"
PASTE_END = b"\x1b[201~"

ARROW_LEFT = b"\x1b[D"
ARROW_RIGHT = b"\x1b[C"
ARROW_UP = b"\x1b[A"
ARROW_DOWN = b"\x1b[B"
HOME = b"\x1b[H"
END = b"\x1b[F"
PAGE_UP = b"\x1b[5~"
PAGE_DOWN = b"\x1b[6~"
F1 = b"\x1bOP"
CTRL_N = b"\x0e"
CTRL_R = b"\x12"
CTRL_U = b"\x15"
CTRL_Y = b"\x19"


class TermTester:
    """Runs the term under a pty and reads back what it painted."""

    def __init__(self, cli_run_path: str, config_yaml: str, columns: int = 100, rows: int = 40) -> None:
        """Record the binary, the config and the terminal geometry to report to the child."""
        self.cli_run_path: str = cli_run_path
        self.config_yaml: str = config_yaml
        self.columns: int = columns
        self.rows: int = rows
        self.pid: int = -1
        self.fd: int = -1
        self._read: str = ""

    @staticmethod
    def _strip_ansi(text: str) -> str:
        """Remove the escape sequences, leaving the characters that were on the screen."""
        return re.sub(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])", "", text)

    def start(self, timeout: float = 6.0) -> str:
        """Fork a pty, start the kernel, and read until the prompt appears."""
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            # os._exit, not sys.exit: this is a forked child with the test module loaded, so an execv
            # that raises -- a missing binary, wrong permissions -- would let the exception escape into
            # unittest and run the whole suite a second time inside the child, writing its results into
            # the pty for the parent to read as screen content. The parent's assertion fails either way;
            # this keeps the failure legible.
            try:
                os.environ["TERM"] = "xterm-256color"
                os.execv(self.cli_run_path, [self.cli_run_path, self.config_yaml])
            except BaseException:  # noqa: BLE001 - the child must not continue, whatever went wrong
                os._exit(1)
            os._exit(1)

        # The renderer asks the terminal for its size; a pty starts at 0x0.
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", self.rows, self.columns, 0, 0))
        self.wait_for("sen:", timeout=timeout)
        return self.screen()

    def read_output(self, timeout: float = 0.6, quiet: float = 0.2) -> str:
        """Read what the term paints, for at most `timeout` seconds.

        Bounded by wall time and not only by silence: the term repaints on a 500 ms idle timer, so a
        reader that waits for the output to stop never returns at all.
        """
        deadline = time.monotonic() + timeout
        chunk = ""
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            ready, _, _ = select.select([self.fd], [], [], min(quiet, remaining))
            if self.fd not in ready:
                break
            try:
                data = os.read(self.fd, 65536)
            except OSError:
                break
            if not data:
                break
            chunk += data.decode(errors="replace")

        self._read += chunk
        return self._strip_ansi(chunk).replace("\r", "\n")

    def screen(self) -> str:
        """Everything painted since the last `forget()`, escape sequences stripped."""
        return self._strip_ansi(self._read).replace("\r", "\n")

    def grid(self) -> VtScreen:
        """The screen as a character grid, for assertions about where something is.

        Built from everything read since the last `forget()`, so the grid holds the latest frame: a
        repaint overwrites the rows the previous frame wrote.
        """
        screen = VtScreen(self.rows, self.columns)
        screen.feed(self._read)
        return screen

    def raw(self) -> str:
        """Everything read since the last `forget()`, escape sequences included."""
        return self._read

    def forget(self) -> None:
        """Drop what has been painted so far.

        Necessary before any assertion about what is on screen *now*: the term repaints the whole
        screen, so a substring that was painted three commands ago is still in the accumulated text
        and an assertion on it passes whatever the current frame says.
        """
        self._read = ""

    def wait_for(self, needle: str, timeout: float = 5.0) -> bool:
        """Read until `needle` has appeared, or the timeout passes."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if needle in self.screen():
                return True
            self.read_output(timeout=0.3)
        return needle in self.screen()

    def send_keys(self, data: bytes, settle: float = 0.6) -> str:
        """Send raw bytes and return what was painted in response."""
        os.write(self.fd, data)
        return self.read_output(timeout=settle)

    def paste(self, text: str, chunk: int = 1024, settle: float = 2.0) -> str:
        """Send text the way a paste arrives, draining the pty as we go.

        One large write would block until the term reads it, and the term is writing to the same pty,
        so a test that writes without draining deadlocks against the program it is testing.
        """
        data = text.encode()
        out = ""
        for offset in range(0, len(data), chunk):
            os.write(self.fd, data[offset : offset + chunk])
            out += self.read_output(timeout=0.1)
        out += self.read_output(timeout=settle)
        return out

    def type_text(self, text: str, settle: float = 0.6) -> str:
        """Type text without submitting it."""
        return self.send_keys(text.encode(), settle=settle)

    def send_command(self, cmd: str, settle: float = 1.0) -> str:
        """Type a command, submit it, and return what was painted in response."""
        return self.send_keys((cmd + "\r").encode(), settle=settle)

    def is_running(self) -> bool:
        """Whether the kernel process is still alive."""
        if self.pid <= 0:
            return False
        try:
            done, _ = os.waitpid(self.pid, os.WNOHANG)
        except ChildProcessError:
            return False
        return done == 0

    def wait_for_exit(self, timeout: float = 8.0) -> bool:
        """Whether the process ends on its own within the timeout."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if not self.is_running():
                return True
            self.read_output(timeout=0.3)
        return not self.is_running()

    def stop(self) -> None:
        """Kill the process if it is still running, and close the pty."""
        if self.pid > 0:
            try:
                os.kill(self.pid, signal.SIGKILL)
                os.waitpid(self.pid, 0)
            except (OSError, ChildProcessError):
                pass
            self.pid = -1
        if self.fd > 0:
            try:
                os.close(self.fd)
            except OSError:
                pass
            self.fd = -1
