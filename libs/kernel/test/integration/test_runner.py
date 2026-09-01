# === test_runner.py ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Keeps the runner able to say how a supporting instance ended.

terminate() on an instance that has already gone is a no-op, so the states are only
separable by asking first.
"""

import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import runner  # noqa: E402


class Instance:
    """A stand-in for one supporting process, with the states Popen can be in."""

    def __init__(self, pid, alive_at_stop, returncode, ignores_terminate=False):
        """Records the state this stand-in should be in when stop() reaches it."""
        self.pid = pid
        self._alive = alive_at_stop
        self.returncode = returncode
        self._ignores = ignores_terminate
        self.killed = False

    def poll(self):
        """None while running, the status once it has ended."""
        return None if self._alive else self.returncode

    def terminate(self):
        """Ends it, unless this stand-in is one that ignores the request."""
        self._alive = self._ignores

    def wait(self, timeout=None):
        """Raises like Popen does when the grace period runs out."""
        if self._alive and timeout is not None:
            raise subprocess.TimeoutExpired("sen", timeout)
        return self.returncode

    def kill(self):
        """Records that the runner had to resort to this."""
        self.killed = True
        self._alive = False


def test_an_instance_that_ended_before_being_asked_is_named_with_its_status(capsys):
    """An instance that went first makes the tester's own exit a consequence."""
    runner.stop([Instance(11, alive_at_stop=False, returncode=66)])
    assert "11 ended on its own with status 66" in capsys.readouterr().out


def test_an_instance_that_ignored_the_request_is_named_as_that(capsys):
    """A different fault: it was alive and would not go."""
    stubborn = Instance(12, alive_at_stop=True, returncode=-9, ignores_terminate=True)
    runner.stop([stubborn])
    assert stubborn.killed
    assert "12 ignored the request to stop and was killed" in capsys.readouterr().out


def test_an_instance_we_stopped_says_nothing(capsys):
    """The common case, which must not become noise."""
    runner.stop([Instance(13, alive_at_stop=True, returncode=-15)])
    assert not capsys.readouterr().out
