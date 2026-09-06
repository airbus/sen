# === inspecting_objects.py ============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import sen

# to store the objects that we want to inspect
list = None


def run():
    global list  # refer to the global variable defined above

    list = sen.api.open("SELECT * FROM local.kernel")  # open it

    # register some callbacks to show changes in the list
    list.onAdded(lambda obj: print(f'Python: object added {obj}'))
    list.onRemoved(lambda obj: print(f'Python: object removed {obj}'))


def update():
    # refer to the global variable defined above
    global list

    print(f"Python: printing the list at: {sen.api.time})")
    print(list)

    print("Python: iterating over the objects in the list:")
    for obj in list:
        print(f"Python: * object {obj.name}")
        print(f"Python:   - class: {obj.className}")
        print(f"Python:   - id:    {obj.id}")
        print(f"Python:   - time:  {obj.lastCommitTime}")
