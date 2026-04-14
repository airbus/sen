# === modify_objects.py ================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import sen

test_object = test_bus = None

cycle = 0

# flag that will be set to True when the onDynamicPropChanged() callback is called
static_value = 56
dynamic_value = 567

def dynamic_prop_changed():
    global test_object, dynamic_value, cb_fired

    assert test_object.dynamicProp == dynamic_value, f"Error in dynamicProp [value: {test_object.dynamicProp}, expectation: {dynamic_value}]"

    # stopping after checking the value of the property
    sen.api.requestKernelStop()

def run():
    global test_object, test_bus, dynamic_value, static_value

    test_object = sen.api.make("py_test_package.TestObject", "test_object", staticProp = static_value)
    test_bus = sen.api.getBus("my.tutorial")
    test_bus.add(test_object)

    # check the value of the static property
    assert test_object.staticProp == static_value, f"Error in staticProp [value: {test_object.staticProp}, expectation: {static_value}]"

    # react to changes in the dynamicProp
    test_object.onDynamicPropChanged(dynamic_prop_changed)

    # set the dynamic prop to a known value
    test_object.dynamicProp = dynamic_value

def update():
    global cycle

    if cycle > 1:
        print("Callback for dynamicProp did not trigger when expected")
        sen.api.requestKernelStop(1)
    cycle += 1

def stop():
    global test_bus, test_object

    test_bus.remove(test_object)
    test_object, test_bus = None, None
