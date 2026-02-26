# === gen_crash_record.py ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import sen
import signal
import os

# to store the object
obj = None
execution_counter = 0

def run():
    global obj  # refer to the global variable defined above
    obj = sen.api.open("SELECT * FROM my.tutorial")


def update():
    global obj  # refer to the global variable defined above
    global execution_counter
    execution_counter += 1

    # if the object is present
    if len(obj) != 0:
        print(f"{obj[0].name}: {obj[0].speed}")
        if execution_counter == 3:
            obj[0].setNextSpeed(123)
        elif execution_counter == 8:
            os.kill(os.getpid(), signal.SIGKILL)
