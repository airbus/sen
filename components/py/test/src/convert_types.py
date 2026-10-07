# === convert_types.py =================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to test what the py component converts between Python and Sen.

FromPythonConverter has an overload per type shape, and before this module only the numeric ones ran:
an enum arrives as its name, a struct as a dict whose size must equal the field count, a sequence as
any Python sequence, an optional as None or a value. The refusals matter as much as the conversions,
because a script that passes the wrong shape is how a user meets them.
"""

import sen

test_object = test_bus = None
cycle = 0


def _expect_refusal(setter, expected_text):
    """Require a set to be refused, and the complaint to say which conversion refused it."""
    try:
        setter()
    except Exception as err:  # noqa: BLE001 - the binding's exception type is not the subject here
        assert expected_text in str(err), f"refused, but not for the reason expected: {err}"
        return
    raise AssertionError(f"the set was accepted, so nothing refused it: expected {expected_text!r}")


def run():
    """Sen run: convert a value of every shape, and require the refusals.

    The conversions are asserted in update() rather than here: a property set from Python is applied by
    the kernel on a later cycle, which is why modify_objects.py checks its value in a change callback
    instead of on the line after the set. The refusals are different — the conversion itself fails, so
    they raise where they are written.
    """
    global test_object, test_bus  # noqa: PLW0603
    test_object = sen.api.make("py_test_package.TestObject", "test_object", staticProp=1)
    test_bus = sen.api.getBus("my.tutorial")
    test_bus.add(test_object)

    test_object.mode = "running"
    test_object.label = "hello"
    test_object.origin = {"x": 3, "y": 4}
    test_object.values = [1, 2, 3]
    test_object.rate = 2.5

    _expect_refusal(lambda: setattr(test_object, "mode", "nosuchmode"), "invalid enumeration")
    _expect_refusal(lambda: setattr(test_object, "origin", {"x": 3}), "differs from field count")


def update():
    """Sen update: once the sets have been applied, require every shape to have survived the round trip.

    Every exit from here asks the kernel to stop, including the failures. An exception raised in update()
    is caught by the py component and logged, and the next cycle runs anyway: a test that signals failure
    by asserting here logs the same error for ever and never ends, which is how this one first hung.
    """
    global cycle  # noqa: PLW0603
    cycle += 1

    if test_object.label != "hello":
        if cycle >= 10:
            print(f"the sets were never applied after {cycle} cycles [label: {test_object.label}]")
            sen.api.requestKernelStop(1)
        return

    wrong = []
    # Asymmetric on purpose or not, this is what it does: an enum is written by name and read back as
    # its integer key, so "running" goes in and 1 comes out.
    if test_object.mode != 1:
        wrong.append(f"mode is {test_object.mode!r}")
    if test_object.origin["x"] != 3 or test_object.origin["y"] != 4:
        wrong.append(f"origin is {test_object.origin}")
    if list(test_object.values) != [1, 2, 3]:
        wrong.append(f"values is {test_object.values}")
    if test_object.rate is None:
        wrong.append("rate was emptied by a value")

    if wrong:
        print("conversions did not round-trip: " + "; ".join(wrong))
        sen.api.requestKernelStop(1)
        return

    sen.api.requestKernelStop()


def stop():
    """Sen stop: trigger that the execution stops."""
    global test_bus, test_object  # noqa: PLW0603
    test_bus.remove(test_object)
    test_object, test_bus = None, None
