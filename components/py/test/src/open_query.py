# === open_query.py ====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to test opening queries through the py component."""

import sen

test_object = test_bus = test_query = None

cycle = 0


def added_callback(_obj):
    """Callback to react when an object is added."""
    """A workaround was created for testing some asserts not exiting the program when they failed"""
    try:
        assert len(test_query) == 1
    except AssertionError:
        print(f"Expected result after adding the object to be 1, got: {len(test_query)}")
        sen.api.requestKernelStop(1)


def removed_callback(_obj):
    """Callback to react when an object is removed."""
    """A workaround was created for testing some asserts not exiting the program when they failed"""
    try:
        assert len(test_query) == 0
    except AssertionError:
        print(f"Expected result after removing the object to be 0, got: {len(test_query)}")
        sen.api.requestKernelStop(1)

    # stopping after checking the object has been removed
    sen.api.requestKernelStop()


def run():
    """Sen run: to set up the initial component state."""
    global test_object, test_bus, test_query  # noqa: PLW0603

    # check the value of the app name
    test_app_name = sen.api.appName
    assert test_app_name == "open_query", f"Error in appName [value: {test_app_name}, expectation: open_query]"

    test_bus = sen.api.getBus("my.test")
    sen.api.syncCalls = True

    # check the component throws an exception when trying to make an incomplete query
    try:
        sen.api.open("SELECT * FROM ")
        raise AssertionError("Error in open: expected a RuntimeError for an incomplete query")
    except RuntimeError:
        pass

    test_query = sen.api.open("SELECT * FROM my.test")

    # react to adding objects
    test_query.onAdded(added_callback)

    # react to removing objects
    test_query.onRemoved(removed_callback)

    test_object = sen.api.make("py_open_query.OpenQueryObject", "test_object")
    test_bus.add(test_object)


def update():
    """Sen update: triggers test execution."""
    global cycle  # noqa: PLW0603

    if cycle == 0:
        test_bus.remove(test_object)

    if cycle > 2:
        print("Callback for adding an object or removing it did not trigger when expected")
        sen.api.requestKernelStop(1)
    cycle += 1


def stop():
    """Sen stop: trigger that the execution stops."""
    global test_bus, test_object  # noqa: PLW0603

    test_object, test_bus = None, None
