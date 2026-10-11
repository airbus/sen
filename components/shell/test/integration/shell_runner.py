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
import socket
import sys
import tempfile
from pathlib import Path


def _free_port() -> int:
    """A port the operating system says is free right now."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


# The discovery port these tests run on, chosen once per test process. They used to share the
# 65454 their configs name, so two of them at once bound it and one saw the other's objects:
# that is what failed as shell_ls_open_bus under `ctest --parallel 4`, in 0.06 seconds, on run
# 38096353194. It passed on the leg beside it, because --schedule-random decides whether any
# two of them overlap.
#
# Once per process rather than per tester, because log_prompt starts a shell and a second
# process that have to meet on the same hub. SEN_TEST_HUB_PORT overrides it, for running one
# test against a port of your choosing.
HUB_PORT = int(os.environ.get("SEN_TEST_HUB_PORT") or _free_port())

# A port written as a number, which is the whole of what two tests cannot share.
_PORT_LINE = re.compile(r"^(\s*(?:port|runDiscoveryHub):\s*)\d+\s*$", re.MULTILINE)
_RENDERED = Path(tempfile.mkdtemp(prefix="sen-shell-config-"))


def config_on_this_run_s_port(config_yaml: str) -> str:
    """Copies a config with its discovery port replaced, returning the copy's path.

    The checked-in file keeps a literal port: it stays valid yaml, which `check-yaml` requires
    and an @env placeholder would break, and it stays runnable by hand. The copy is what a test
    actually launches. These configs name no relative paths, so the copy works anywhere.
    """
    source = Path(config_yaml)
    rendered, count = _PORT_LINE.subn(rf"\g<1>{HUB_PORT}", source.read_text(encoding="utf-8"))
    if count == 0:
        raise RuntimeError(f"{source} names no discovery port, so this test cannot be isolated")
    destination = _RENDERED / f"{source.parent.parent.name}-{source.name}"
    destination.write_text(rendered, encoding="utf-8")
    return str(destination)


class ShellTester:
    """Interacts with the shell using a pseudo terminal."""

    def __init__(self, cli_run_path: str, config_yaml: str) -> None:
        """Initialize the shell tester configuration."""
        self.cli_run_path: str = cli_run_path
        self.config_yaml: str = config_on_this_run_s_port(config_yaml)
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
