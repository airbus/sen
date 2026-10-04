# === influx_source.py =================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module that publishes an object of every value shape, for the influx component to record."""

import sen

test_object = test_bus = None

cycle = 0


def _set(name, value):
    """Sets one property, as a callable the table below can hold."""
    return lambda: setattr(test_object, name, value)


# One shape per cycle rather than all at once: the influx component samples on its own period, and
# spreading the changes gives it something to read on more than one sample.
#
# Dividing by zero makes the object fire its event, which is recorded as a point of its own. Taking
# the object off the bus and putting it back is what a component does when it stops and starts, and
# the recorder has a path of its own for each.
CHANGES: dict[int, object] = {}


def run():
    """Sen run: publishes the object the influx selection picks up."""
    global test_object, test_bus  # noqa: PLW0603

    test_object = sen.api.make("py_test_package.TestObject", "recorded_object", staticProp=7)
    test_bus = sen.api.getBus("my.tutorial")
    test_bus.add(test_object)

    CHANGES.update(
        {
            1: _set("dynamicProp", 1.5),
            2: _set("mode", "running"),
            3: _set("label", "recorded"),
            4: _set("origin", {"x": 3, "y": 4}),
            5: _set("values", [1, 2, 3]),
            6: _set("rate", 2.5),
            7: lambda: test_object.divide(1.0, 0.0),
            20: lambda: test_bus.remove(test_object),
            30: lambda: test_bus.add(test_object),
        }
    )


def update():
    """Sen update: changes every property in turn, so each type shape is recorded.

    One shape per cycle rather than all at once: the influx component samples on its own period,
    and spreading the changes gives it something to read on more than one sample.
    """
    global cycle  # noqa: PLW0603

    cycle += 1
    if cycle in CHANGES:
        CHANGES[cycle]()
    elif cycle > 60:
        sen.api.requestKernelStop()


def stop():
    """Sen stop: trigger that the execution stops."""
    global test_bus, test_object  # noqa: PLW0603

    test_object, test_bus = None, None
