# === event.py =========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to test reacting to events through the py component."""

import sen

test_object = test_bus = test_query = None

result = 1.0
static_value = cycle = 0


def event_triggered(_obj):
    """Callback to react when an event occurs."""
    """A workaround was created for testing some asserts not exiting the program when they failed"""
    try:
        assert result == 3.0
    except AssertionError:
        print(f"Expected result after event triggered to be 3.0, got: {result}")
        sen.api.requestKernelStop(1)

    sen.api.requestKernelStop()


def run():
    """Sen run: to set up the initial component state."""
    global test_object, test_bus, test_query, result  # noqa: PLW0603

    sen.api.syncCalls = True

    test_object = sen.api.make("py_test_package.TestObject", "test_object", staticProp=static_value)
    test_bus = sen.api.getBus("my.test")
    test_bus.add(test_object)

    test_query = sen.api.open("SELECT * FROM my.test")

    # react to ondivisionByZero event
    test_object.ondivisionByZero(event_triggered)

    result = test_query[0].divide(6.0, 2.0)


def update():
    """Sen update: triggers test execution."""
    global cycle, result  # noqa: PLW0603

    if cycle == 0:
        try:
            assert result == 3.0
            result = test_query[0].divide(6.0, 0.0)
        except AssertionError:
            print(f"Expected result after first division to be 3.0, got: {result}")
            sen.api.requestKernelStop(1)

    if cycle > 2:
        print("Callback for divisionByZero did not trigger when expected")
        sen.api.requestKernelStop(1)
    cycle += 1


def stop():
    """Sen stop: trigger that the execution stops."""
    global test_bus, test_object  # noqa: PLW0603

    test_bus.remove(test_object)
    test_object, test_bus = None, None
