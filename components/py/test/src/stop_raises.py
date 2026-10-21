# === stop_raises.py ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to test that an exception raised by stop() is reported and does not crash the process."""

import sen

cycle = 0


def update():
    """Asks the kernel to stop after a couple of cycles, so that stop() runs."""
    global cycle  # noqa: PLW0603
    cycle += 1
    if cycle == 2:
        sen.api.requestKernelStop()


def stop():
    """Raises, as a module with a bug in its clean-up would."""
    raise RuntimeError("raised from stop() on purpose")
