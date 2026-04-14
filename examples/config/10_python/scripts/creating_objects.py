# === creating_objects.py ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import sen

myObject = testBus = None

def run():
    global myObject, testBus  # refer to the globals defined above

    type = {
        "entityKind": 1,
        "domain": 2,
        "countryCode": 198,
        "category": 1,
        "subcategory": 3,
        "specific": 0,
        "extra": 0
    }

    id = {
        "entityNumber": 1,
        "federateIdentifier": {
            "siteID": 1,
            "applicationID": 1
        }
    }

    print(f"Python: creating and publishing the object")
    myObject = sen.api.make("aircrafts.DummyAircraft", "myAircraft", entityType=type, alternateEntityType=type, entityIdentifier = id)
    testBus = sen.api.getBus("my.tutorial")
    testBus.add(myObject)

    # setting the speed property to 150
    myObject.speed = 150

def update():
    print(myObject)

def stop():
    global testBus, myObject  # refer to the globals defined above

    print("Python: deleting the object")
    testBus.remove(myObject)
    myObject = None
    testBus = None
