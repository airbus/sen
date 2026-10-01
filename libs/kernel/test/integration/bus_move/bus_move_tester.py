# === bus_move_tester.py ===============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to end the bus move test as a Sen component.

The verdict is the watcher's, matched by ctest against this test's output. This only gives the
other two processes long enough to finish and then stops everything, so a passing run ends on its
own rather than on the runner's timeout.
"""

import sen
from tester import TesterBase

# Longer than the watcher's own deadline, so that a run which fails still prints its verdict
# before anything is torn down.
SECONDS_BEFORE_SHUTDOWN = 4.0


class BusMoveTester(TesterBase):
    """Tester class to end the bus move test."""

    def set_tests(self):
        """Registers the test functions."""

        def test_condition():
            return self.get_test_elapsed_seconds() > SECONDS_BEFORE_SHUTDOWN

        def test_body():
            for obj in object_list:
                if obj.name in ("mover", "watcher"):
                    obj.shutdownKernel()

            sen.api.requestKernelStop(0)

        self.set_test("bus_move_test", test_body, test_condition)


tester = None
object_list = None


def run():
    """Sen run: to setup the initial component state."""
    global tester, object_list  # noqa: PLW0603
    object_list = sen.api.open("SELECT * FROM session.ctrl")
    tester = BusMoveTester("bus_move_tester", sen.api)
    tester.set_tests()


def update():
    """Sen update: triggers test execution."""
    tester.run_tests()
