# === reacting_to_events_and_changes.py ================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import sen

# to store the objects
teachers = None


def teacherDetected(teacher):
    teacher.onStressLevelPeaked(lambda args: print(f"Python: {teacher.name} stress level peaking to {args}"))
    teacher.onStatusChanged(lambda: print(f"Python: {teacher.name} status changed to {teacher.status}"))


def run():
    global teachers  # refer to the global variable defined above

    print("Python: run")

    # select the object and install some callbacks
    teachers = sen.api.open("SELECT school.Teacher FROM school.primary")
    teachers.onAdded(lambda obj: teacherDetected(obj))
