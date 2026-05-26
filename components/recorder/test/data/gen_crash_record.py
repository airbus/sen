# === gen_crash_record.py ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to generate crash records."""

import os
import signal

import sen

# to store the object
obj = None
execution_counter = 0


def run():
    """Sen run: to setup the initial component state."""
    global obj  # refer to the global variable defined above  # noqa: PLW0603
    obj = sen.api.open("SELECT * FROM my.tutorial")


def update():
    """Sen update: triggers test execution."""
    global execution_counter  # refer to the global variable defined above  # noqa: PLW0603
    execution_counter += 1

    # if the object is present
    if len(obj) != 0:
        print(f"{obj[0].name}: {obj[0].speed}")
        if execution_counter == 3:
            obj[0].setNextSpeed(123)
        elif execution_counter == 8:
            os.kill(os.getpid(), signal.SIGKILL)
