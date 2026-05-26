# === type_clash_tester.py =============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to run the type clash tests as a Sen component."""

import sen
from tester import TesterBase


class TypeClashTester(TesterBase):
    """Tester class to execute the type clash tests."""

    def set_tests(self):
        """Registers the test functions."""

        def test_condition():
            return self.get_test_elapsed_seconds() > 2.5

        def test_body():
            for obj in object_list:
                if "obj_app_" in obj.name:
                    obj.shutdownKernel()

            sen.api.requestKernelStop(0)

        self.set_test("clash_test", test_body, test_condition)


tester = None
object_list = None


def run():
    """Sen run: to setup the initial component state."""
    # TODO (SEN-1689): clean up global state dependence
    global tester, object_list  # noqa: PLW0603
    object_list = sen.api.open("SELECT * FROM session.bus")
    tester = TypeClashTester("type_clash_tester", sen.api)
    tester.set_tests()


def update():
    """Sen update: triggers test execution."""
    tester.run_tests()
