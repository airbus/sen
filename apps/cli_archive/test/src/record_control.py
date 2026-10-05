# === record_control.py ================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to drive the recording the cli_archive checks read, and to stop once it holds something."""

from datetime import timedelta

import sen

# Bounds a failure rather than measuring anything: the objects are published by the kernel itself
# and are normally there before this module runs at all.
APPEARED = timedelta(seconds=30)

# Named once, so the message below cannot drift from what is actually being waited on.
SELECTION = "SELECT * FROM local.kernel"

# Cycles to keep recording after the objects are on the bus. At the configured rate these are a few
# keyframe periods, so the archive carries an object index and not only the start of a stream.
RECORDING_CYCLES = 10

query = None
cycle = 0


def run():
    """Sen run: wait for the objects the recording selects before letting it run on."""
    global query  # noqa: PLW0603

    # `sen run --start-stop` stops once every component is running, which says nothing about the
    # recorder having seen anything: on CI it produced an archive with an empty object index, and
    # on one lane no archive at all. Waiting for the objects here is what makes it non-empty every
    # time, and says so when they never arrive instead of recording nothing and passing.
    query = sen.api.open(SELECTION)
    if not query.waitUntilNotEmpty(APPEARED):
        print(f"nothing matched {SELECTION} within {APPEARED}, so there is nothing to record")
        sen.api.requestKernelStop(1)


def update():
    """Sen update: gives the recorder a few keyframes of the objects, then ends the run."""
    global cycle  # noqa: PLW0603

    cycle += 1
    if cycle >= RECORDING_CYCLES:
        sen.api.requestKernelStop()


def stop():
    """Sen stop: trigger that the execution stops."""
    global query  # noqa: PLW0603

    query = None
