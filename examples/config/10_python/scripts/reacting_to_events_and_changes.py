# === reacting_to_events_and_changes.py ================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Example module that demonstrates how to react on events and changes (e.g., object added)."""

import sen

# to store the objects
teachers = None


def teacherDetected(teacher) -> None:
    """Prints information about the detected teacher."""
    teacher.onStressLevelPeaked(lambda args: print(f"Python: {teacher.name} stress level peaking to {args}"))
    teacher.onStatusChanged(lambda: print(f"Python: {teacher.name} status changed to {teacher.status}"))


def run() -> None:
    """Sen run: to setup the initial component state."""
    global teachers  # refer to the global variable defined above  # noqa: PLW0603

    print("Python: run")

    # select the object and install some callbacks
    teachers = sen.api.open("SELECT school.Teacher FROM school.primary")
    teachers.onAdded(teacherDetected)
