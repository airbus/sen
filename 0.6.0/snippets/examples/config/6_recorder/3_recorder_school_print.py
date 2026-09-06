# === 3_recorder_school_print.py =======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import sen_db_python as sen
from datetime import datetime

epoch = datetime(1970,1,1)

try:
    input = sen.Input("school_recording")
    print(f"Opened archive '{input.path}' with the following summary:")
    print(f" - firstTime: {epoch+input.summary.firstTime}")
    print(f" - lastTime: {epoch+input.summary.lastTime}")
    print(f" - keyframeCount: {input.summary.keyframeCount}")
    print(f" - objectCount: {input.summary.objectCount}")
    print(f" - typeCount: {input.summary.typeCount}")
    print(f" - annotationCount: {input.summary.annotationCount}")
    print(f" - indexedObjectCount: {input.summary.indexedObjectCount}")
    print("")

    print("Iterating over the entries:")
    print("")

    cursor = input.begin()
    while not cursor.atEnd:
        entry = cursor.entry

        if type(entry.payload) is sen.Keyframe:
            print(f"{epoch+entry.time} -> Keyframe:")
            for obj in entry.payload.snapshots:
                print(f"  - object: {obj.name}")
                print(f"    class:  {obj.className}")
            print("")

        elif type(entry.payload) is sen.Creation:
            print(f"{epoch+entry.time} -> Object Creation:")
            print(f"  - name:  {entry.payload.name}")
            print(f"  - class: {entry.payload.className}")
            print("")

        elif type(entry.payload) is sen.Deletion:
            print(f"{epoch+entry.time} -> Object Deletion:")
            print(f"  - object id:  {entry.payload.objectId}")
            print("")

        elif type(entry.payload) is sen.PropertyChange:
            print(f"{epoch+entry.time} -> Property Changed:")
            print(f"  - object id:  {entry.payload.objectId}")
            print(f"  - property:   {entry.payload.name}")
            print("")

        elif type(entry.payload) is sen.Event:
            print(f"{epoch+entry.time} -> Event:")
            print(f"  - object id:  {entry.payload.objectId}")
            print(f"  - event:      {entry.payload.name}")
            print("")

        cursor.advance()

except Exception as err:
    print(err)
