# === logmaster_control.py =============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to drive the logmaster component, which has no terminal in a test."""

import sen

query = None
cycle = 0
drove_master = drove_logger = False


def fail(message):
    """Reports what did not happen and ends the run non-zero."""
    print(message)
    sen.api.requestKernelStop(1)


# Every level the component maps, so both directions of the mapping are exercised: one switch
# turns a Sen level into an spdlog level when it is set, the other turns it back when it is read.
LEVELS = ("trace", "debug", "info", "warn", "err", "critical", "off")


def drive_master(obj):
    """Mutes and unmutes every logger at once, and sets every level for all of them."""
    global drove_master  # noqa: PLW0603

    obj.muteAll()
    obj.unmuteAll()
    obj.toggleMuteAll()
    obj.toggleMuteAll()
    for level in LEVELS:
        obj.setLevel(level)
    drove_master = True


def drive_logger(obj):
    """Mutes one logger, puts it back, and takes it through every level and a new pattern."""
    global drove_logger  # noqa: PLW0603

    obj.mute()
    obj.unmute()
    for level in LEVELS:
        obj.setLevel(level)
        if not str(obj.level):
            fail(f"the logger reported no level after being set to {level}")
    obj.setPattern("[%n] %v")
    drove_logger = True


def added(obj):
    """Callback for each object the logmaster publishes on its bus.

    Which class it is, asked of the object rather than read out of its name: the package is
    called logmaster, so every Logger has the word master in its class name too.
    """
    if hasattr(obj, "muteAll"):
        drive_master(obj)
    else:
        drive_logger(obj)


def run():
    """Sen run: to set up the initial component state."""
    global query  # noqa: PLW0603

    sen.api.syncCalls = True
    query = sen.api.open("SELECT * FROM local.log")
    query.onAdded(added)


def update():
    """Sen update: gives the logmaster time to scan, then checks both were driven."""
    global cycle  # noqa: PLW0603

    cycle += 1
    # The logmaster scans for loggers once a period, so the objects do not exist on the first
    # cycle. Twenty cycles at the configured rate is comfortably longer than one scan.
    if cycle < 20:
        return

    if not drove_master:
        fail("the logmaster published no master on local.log")
    elif not drove_logger:
        fail("the logmaster published no logger on local.log")
    else:
        sen.api.requestKernelStop()


def stop():
    """Sen stop: trigger that the execution stops."""
    global query  # noqa: PLW0603

    query = None
