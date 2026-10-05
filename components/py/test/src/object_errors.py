# === object_errors.py =================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to test how the py component refuses an object used wrongly."""

import sen

test_object = test_bus = None


def expect(what, error, action):
    """Runs an action that has to raise, and fails the run when it does not.

    An assert in this component does not end the process, so a failure is reported the way the
    other modules here report one: print what was expected and ask the kernel to stop non-zero.
    """
    try:
        action()
    except error:
        return
    except Exception as unexpected:  # noqa: BLE001
        print(f"{what}: expected {error.__name__}, got {type(unexpected).__name__}: {unexpected}")
        sen.api.requestKernelStop(1)
        return

    print(f"{what}: expected {error.__name__}, but nothing was raised")
    sen.api.requestKernelStop(1)


def run():
    """Sen run: to set up the initial component state."""
    global test_object, test_bus  # noqa: PLW0603

    test_object = sen.api.make("py_test_package.TestObject", "test_object", staticProp=1)
    test_bus = sen.api.getBus("my.tutorial")
    test_bus.add(test_object)

    # An object is hashable, so Python can hold it in a set or use it as a dictionary key.
    assert hash(test_object) == hash(test_object), "an object did not hash to the same value twice"

    expect("reading a member that does not exist", AttributeError, lambda: test_object.notAMember)
    expect("writing a property that does not exist", AttributeError, lambda: setattr(test_object, "notAProp", 1))
    expect("writing a property that is not writable", AttributeError, lambda: setattr(test_object, "staticProp", 2))
    # The component takes exactly one argument here and says so; it does not check that the
    # argument is callable, which is what running this against a string showed.
    expect("a property callback given no argument", ValueError, test_object.onDynamicPropChanged)
    expect("an event callback given two arguments", ValueError, lambda: test_object.onDivisionByZero(run, run))
    expect("a method given too few arguments", AttributeError, lambda: test_object.divide(1.0))

    sen.api.requestKernelStop()


def stop():
    """Sen stop: trigger that the execution stops."""
    global test_bus, test_object  # noqa: PLW0603

    test_bus.remove(test_object)
    test_object, test_bus = None, None
