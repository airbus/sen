# === compat_mode_tester.py ============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks what a compatibility mode does to an object whose type converts with loss.

The other participant publishes the same class with a wider property, so reading it here can drop a
value. Whether that object is allowed onto the bus is the whole question, so this asserts the object
rather than a log line: a test that matched a message would pass with the decision inverted.

SEN_EXPECT_CLASH_OBJECT says which outcome this run expects, 1 for accepted and 0 for refused.
"""

import os

import sen
from tester import TesterBase


class CompatModeTester(TesterBase):
    """Asserts whether the mismatched object reached this kernel."""

    def set_tests(self):
        """Registers the test functions."""

        def test_condition():
            return self.get_test_elapsed_seconds() > 2.5

        def test_body():
            expected = os.environ.get("SEN_EXPECT_CLASH_OBJECT") == "1"
            names = [obj.name for obj in object_list]
            arrived = any("obj_clash" in name for name in names)

            if arrived != expected:
                print(f"FAILED: expected obj_clash arrived={expected}, got {arrived}. Objects: {names}")
                self.mark_as_failed()
            else:
                print(f"OK: obj_clash arrived={arrived}, as expected")

            for obj in object_list:
                if "obj_app_" in obj.name:
                    obj.shutdownKernel()

            sen.api.requestKernelStop(0)

        self.set_test("compat_mode_test", test_body, test_condition)


tester = None
object_list = None


def run():
    """Sen run: to setup the initial component state."""
    # TODO (SEN-1689): clean up global state dependence
    global tester, object_list  # noqa: PLW0603
    object_list = sen.api.open("SELECT * FROM session.bus")
    tester = CompatModeTester("compat_mode_tester", sen.api)
    tester.set_tests()


def update():
    """Sen update: triggers test execution."""
    tester.run_tests()
