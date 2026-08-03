# === shell_runner.py ==================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""Module containing helper classes to run and interact with the shell component."""

import os
import pty
import re
import select
import signal
import sys


class ShellTester:
    """Interacts with the shell using a pseudo terminal."""

    def __init__(self, cli_run_path: str, config_yaml: str) -> None:
        """Initialize the shell tester configuration."""
        self.cli_run_path: str = cli_run_path
        self.config_yaml: str = config_yaml
        self.pid: int = -1
        self.fd: int = -1

    def _strip_ansi(self, text: str) -> str:
        """Removes ANSI escape sequences from the shell output to make text assertions."""
        ansi_escape = re.compile(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])")
        return ansi_escape.sub("", text)

    def start(self) -> str:
        """Forks a new terminal and starts the shell, returning the initial output."""
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execv(self.cli_run_path, [self.cli_run_path, self.config_yaml])
            sys.exit(1)

        return self.read_output()

    def read_output(self, timeout: float = 0.5) -> str:
        """Reads all the shell output until timeout."""
        output = ""
        while True:
            ready, _, _ = select.select([self.fd], [], [], timeout)
            if self.fd in ready:
                try:
                    data = os.read(self.fd, 4096).decode(errors="replace")
                    if not data:
                        break
                    output += data
                except OSError:
                    break
            else:
                break

        return self._strip_ansi(output).replace("\r", "\n")

    def send_command(self, cmd: str) -> str:
        """Sends a command to the shell and returns the resulting output."""
        os.write(self.fd, (cmd + "\r").encode())
        return self.read_output()

    def stop(self) -> None:
        """Kills the shell process and cleans up."""
        if self.pid and self.pid > 0:
            try:
                os.kill(self.pid, signal.SIGKILL)
            except OSError:
                pass
            os.close(self.fd)
