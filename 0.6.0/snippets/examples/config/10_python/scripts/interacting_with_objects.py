# === interacting_with_objects.py ======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import sen

# to store the object
obj = None


def run():
    global obj  # refer to the global variable defined above
    obj = sen.api.open("SELECT * FROM local.shell WHERE name = \"shell_impl\"")


def update():
    global obj  # refer to the global variable defined above
    print("Python: update")

    # if the object is present, do something with it
    if len(obj) != 0:
        print("Python: interacting with the object")
        obj[0].info("i16")  # print the info of the i16 type
        obj[0].ls()  # list the current objects in the terminal
        obj[0].history()  # show the current history

        print("Python: asking the process to shut down")
        obj[0].shutdown()  # trigger the process shutdown
