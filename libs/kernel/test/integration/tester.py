# === tester.py ========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to run the generic integration testes as a Sen component."""

import sen


class TesterBase:
    """Tester base for generic testing functions."""

    def __init__(self, name, sen_api):
        """
        Base testing class to reproduce tests on an iterative loop inside a Python Sen component.

        Args:
            name: Name of the test to run
            sen_api: Python Sen api used in the component.
        """
        self._name = name
        self.__api = sen_api
        self.__start_time = self.__api.time
        self.__tests = {}
        self.__test_conditions = {}
        self.__have_tests_failed = {}
        self.__have_tests_run = {}

        self.__is_finished = False
        self.__has_failed = False

    def mark_as_failed(self):
        """Marks the tests state as failed and stops test execution."""
        self.__has_failed = True
        self.__api.requestKernelStop(1)

    def get_test_elapsed_seconds(self):
        """Get the seconds passed since the beginning of the test."""
        if self.__start_time.total_seconds() == 0:
            self.__start_time = self.__api.time
        return (self.__api.time - self.__start_time).total_seconds()

    def set_test(self, test_name, test_func, test_condition):
        """
        Create a test to be run during the testing process.

        Args:
            test_name: Name of the test
            test_func: Function that contains the logic of the test to execute
            test_condition: Condition which will trigger the test.
        """
        self.__tests[test_name] = test_func
        self.__test_conditions[test_name] = test_condition
        self.__have_tests_failed[test_name] = False  # Test has not failed yet
        self.__have_tests_run[test_name] = False  # Test has not been run yet

    def execute_test(self, test_name):
        """
        Execute a given test if it hasn't been run already and its condition has been met.

        Store test result inside the test dictionary and manage the assertion error in case the test fails.
        """
        if (
            not self.__have_tests_run.get(test_name) and self.__test_conditions[test_name]()
        ):  # Test hasn't been run and its condition is met
            try:
                self.__tests[test_name]()  # Execute the test function
                self.__have_tests_run[test_name] = True  # Mark as executed
            except AssertionError as e:
                print(f"{self._name} -- {test_name}: Test failed with exception:\n {e}")
                self.__have_tests_failed[test_name] = True

    def run_tests(self):
        """
        Iterative method that runs the specified tests.

        This methods is checking the possible failures on assertions and marking the component to stop in failure case.
        """
        for test_name in self.__tests.keys():
            self.execute_test(test_name)
            if self.__have_tests_failed[test_name]:
                self.mark_as_failed()


class KernelTransportTester(TesterBase):
    """Tester class to execute the kernel transport tests."""

    def set_tests(self):
        """Registers the test functions."""

        def test_condition():
            """Check that both tester objects are ready prior to executing the test."""
            # TODO (SEN-1689): clean up global state dependence
            global tester1, tester2  # noqa: PLW0603

            expected_names = {"tester1", "tester2", "obj1", "obj2"}
            objects_present = len(object_list) == 4 and {obj.name for obj in object_list} == expected_names
            tester1 = next((obj for obj in object_list if obj.name == "tester1"), None)
            tester2 = next((obj for obj in object_list if obj.name == "tester2"), None)
            testers_ready = tester1 is not None and tester2 is not None and tester1.ready and tester2.ready

            print(
                f"[tester] checking test condition: objects_present: {objects_present} , testers_ready {testers_ready}"
            )

            if objects_present and not testers_ready:
                print(f"tester1: {tester1.ready}")
                print(f"tester2: {tester2.ready}")

            return objects_present and testers_ready

        def abort_tests():
            tester1.shutdownKernel()
            tester2.shutdownKernel()
            self.mark_as_failed()

        def check_test_4(result):
            assert not result, abort_tests()
            tester1.shutdownKernel()
            tester2.shutdownKernel()
            sen.api.requestKernelStop(0)

        def check_test_3(result):
            assert not result, abort_tests()
            print("[tester] calling tester1.checkLocalState")
            tester1.checkLocalState(check_test_4)

        def check_test_2(result):
            assert not result, abort_tests()
            print("[tester] calling tester2.doTests")
            tester2.doTests(check_test_3)

        def check_test_1(result):
            assert not result, abort_tests()
            print("[tester] calling tester2.checkLocalState")
            tester2.checkLocalState(check_test_2)

        def test_body():
            """Check setting of remote properties between two kernel instances."""
            # test setting remote properties in the forward direction
            print("[tester] calling tester1.doTests")
            tester1.doTests(check_test_1)

        self.set_test("transport_test", test_body, test_condition)


# global variables
tester = None
object_list = None
tester1 = None
tester2 = None


def run():
    """Sen run: to setup the initial component state."""
    # TODO (SEN-1689): clean up global state dependence
    global tester, object_list  # noqa: PLW0603

    object_list = sen.api.open("SELECT * FROM session.bus")
    print("[tester] creating obj list with interest SELECT * FROM session.bus")

    tester = KernelTransportTester("transport_tester", sen.api)
    tester.set_tests()


def update():
    """Sen update: triggers test execution."""
    tester.run_tests()
