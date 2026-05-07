# === type_clash_tester.py =============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import sen
from tester import TesterBase

class TypeClashTester(TesterBase):
    def set_tests(self):
        def test_condition():
            return self.get_test_elapsed_seconds() > 2.5

        def test_body():
            global object_list

            for obj in object_list:
                if "obj_app_" in obj.name:
                    try:
                        obj.shutdownKernel()
                    except:
                        pass

            sen.api.requestKernelStop(0)

        self.set_test("clash_test", test_body, test_condition)

tester = None
object_list = None

def run():
    global tester, object_list
    object_list = sen.api.open("SELECT * FROM session.bus")
    tester = TypeClashTester("type_clash_tester", sen.api)
    tester.set_tests()

def update():
    global tester
    tester.run_tests()
