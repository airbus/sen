# === hello_python.py ==================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Example module that demonstrates how to use the python component."""

import sen


# this is executed only once (at the start of the component execution)
def run():
    """Sen run: to setup the initial component state."""
    print("Python: run")
    print(f"Python: the config is: {sen.api.config}")
    print(f"Python: the app name is: {sen.api.appName}")


# this is executed every cycle
def update():
    """Sen update: triggers test execution."""
    print(f"Python: update (current time: {sen.api.time})")


# this is executed only once (at the end of the component execution)
def stop():
    """Sen stop: trigger that the execution stops."""
    print("Python: stop called")
