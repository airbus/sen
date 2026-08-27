# === wait_until.py ====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to test the waitUntil family of calls through the py component."""

import threading
from datetime import timedelta

import sen

test_bus = test_query = None

test_objects = []

cycle = 0


def change_condition():
    """Condition that is always met, used to check waitUntil returns immediately."""
    return True


def add_object(name):
    """Create an object with the given name and add it to the test bus."""
    test_object = sen.api.make("py_open_query.OpenQueryObject", name)
    test_bus.add(test_object)
    test_objects.append(test_object)


def remove_object(position):
    """Remove the object at the given position from the test bus."""
    test_bus.remove(test_objects[position])


def run():
    """Sen run: to set up the initial component state."""
    global test_bus, test_query  # noqa: PLW0603

    # check waitUntil unblocks before the timeout is reached
    timeout = timedelta(seconds=1)
    not_timed_out = sen.api.waitUntil(change_condition, timeout)
    assert not_timed_out is True, f"waitUntil timed out after {timeout}s waiting for change_condition"

    test_bus = sen.api.getBus("my.test")

    # check the query has no objects added at the start
    test_query = sen.api.open("SELECT * FROM my.test")
    assert len(test_query) == 0, f"test_query length: {test_query}"

    # create and start a new thread to add an object
    t = threading.Thread(target=add_object, kwargs={"name": "test_object1"})
    t.start()

    # check waitUntilNotEmpty unblocks before the timeout is reached
    not_timed_out = test_query.waitUntilNotEmpty()
    assert not_timed_out is True, f"waitUntilNotEmpty timed out after {timeout}s waiting for an object to be added"

    # create and start a new thread to add an object
    t = threading.Thread(target=add_object, kwargs={"name": "test_object2"})
    t.start()

    # check waitUntilSizeIs unblocks before the timeout is reached
    not_timed_out = test_query.waitUntilSizeIs(2)
    assert not_timed_out is True, f"waitUntilNotEmpty timed out after {timeout}s waiting for two objects to be added"

    # create and start new threads to remove objects
    t1 = threading.Thread(target=remove_object, kwargs={"position": 0})
    t2 = threading.Thread(target=remove_object, kwargs={"position": 1})
    t1.start()
    t2.start()

    # check waitUntilEmpty unblocks before the timeout is reached
    not_timed_out = test_query.waitUntilEmpty()
    assert not_timed_out is True, f"waitUntilNotEmpty timed out after {timeout}s waiting for two objects to be removed"


def update():
    """Sen update: triggers test execution."""
    sen.api.requestKernelStop()


def stop():
    """Sen stop: trigger that the execution stops."""
