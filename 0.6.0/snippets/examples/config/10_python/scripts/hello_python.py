# === hello_python.py ==================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import sen

# this is executed only once (at the start of the component execution)
def run():
    print(f"Python: run")
    print(f"Python: the config is: {sen.api.config}")
    print(f"Python: the app name is: {sen.api.appName}")


# this is executed every cycle
def update():
    print(f"Python: update (current time: {sen.api.time})")

# this is executed only once (at the end of the component execution)
def stop():
    print(f"Python: stop called")
