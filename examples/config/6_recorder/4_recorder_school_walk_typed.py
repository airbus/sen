# === 4_recorder_school_walk_typed.py ==================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Tier 2 walk over a `school_recording`.

Beyond `3_recorder_school_print.py` (which prints the entry stream as-is), this one shows:

  - inspecting class shapes via the type registry,
  - reading property *values* (not just names) out of `PropertyChange` entries,
  - reading event *args* out of `Event` entries,
  - reading the per-object property map out of `Keyframe` snapshots,
  - using a per-`Input` `CustomTypeRegistry` so user-defined struct / variant fields come
    back as typed Python objects rather than raw payloads.

Run after `2_recorder_school.yaml` has produced `school_recording`:

    python3 config/6_recorder/4_recorder_school_walk_typed.py
"""

from collections import defaultdict
from datetime import datetime

import sen_db_python as sen


def fmt_value(value):
    """Render a `sen_db_python` value (primitive, KeyedVar dict, struct dict, list) for printing."""
    if value is None:
        return "null"
    if isinstance(value, (bool, int, float, str)):
        return repr(value)
    if isinstance(value, list):
        return "[" + ", ".join(fmt_value(v) for v in value) + "]"
    if isinstance(value, dict):
        # KeyedVar (variant) arrives as exactly {"type": "<qualifiedName>", "value": <payload>}.
        if set(value.keys()) == {"type", "value"}:
            return f"{value['type']}({fmt_value(value['value'])})"
        # Plain struct / VarMap.
        return "{" + ", ".join(f"{k}={fmt_value(v)}" for k, v in value.items()) + "}"
    return repr(value)


epoch = datetime(1970, 1, 1)

try:
    input = sen.Input("school_recording")
    print(
        f"Opened '{input.path}' -- {input.summary.objectCount} objects, "
        f"{input.summary.typeCount} types, {input.summary.keyframeCount} keyframes."
    )
    print()

    # ------------------------------------------------------------------------------------------
    # 1. Schema inspection.
    # ------------------------------------------------------------------------------------------
    # The type registry exposes every type the recording carries. Useful for sanity-checking
    # what classes were exported before you start mining entries.
    print("Types in this recording:")
    for type_name in input.getTypes().classNames:
        print(f"  - {type_name}")
    print()

    # ------------------------------------------------------------------------------------------
    # 2. Walk + tally + typed reads.
    # ------------------------------------------------------------------------------------------
    # Track per-class snapshot counts, per-object property writes, and the last value seen for
    # each property. Read event args too.
    snapshot_count_by_class = defaultdict(int)
    property_changes_by_object = defaultdict(int)
    last_value_by_property = {}  # (objectId, propertyName) -> value
    events_seen = defaultdict(int)  # (objectId, eventName) -> count
    event_arg_samples = {}  # (objectId, eventName) -> first args tuple seen

    cursor = input.begin()
    while not cursor.atEnd:
        entry = cursor.entry
        payload = entry.payload

        if isinstance(payload, sen.Keyframe):
            for obj in payload.snapshots:
                snapshot_count_by_class[obj.className] += 1
                # The snapshot also carries the current property values for that object. Iterate.
                for name in obj.propertyNames:
                    last_value_by_property[(obj.objectId, name)] = getattr(obj, name)

        elif isinstance(payload, sen.PropertyChange):
            property_changes_by_object[payload.objectId] += 1
            last_value_by_property[(payload.objectId, payload.name)] = payload.value

        elif isinstance(payload, sen.Event):
            key = (payload.objectId, payload.name)
            events_seen[key] += 1
            if key not in event_arg_samples:
                event_arg_samples[key] = tuple(payload.args)

        cursor.advance()

    # ------------------------------------------------------------------------------------------
    # 3. Report.
    # ------------------------------------------------------------------------------------------
    print("Snapshot rows by class:")
    for class_name, count in sorted(snapshot_count_by_class.items()):
        print(f"  {class_name}: {count}")
    print()

    print(f"Property writes seen across {len(property_changes_by_object)} objects (top 5):")
    top_objects = sorted(property_changes_by_object.items(), key=lambda kv: -kv[1])[:5]
    for object_id, count in top_objects:
        print(f"  object {object_id}: {count} writes")
    print()

    print(f"Last value seen per property (showing first 10 of {len(last_value_by_property)}):")
    for (object_id, prop_name), value in list(last_value_by_property.items())[:10]:
        print(f"  {object_id}.{prop_name} = {fmt_value(value)}")
    print()

    print(f"Events fired ({sum(events_seen.values())} total across {len(events_seen)} (object, event) pairs):")
    for (object_id, event_name), count in sorted(events_seen.items(), key=lambda kv: -kv[1])[:5]:
        args = event_arg_samples.get((object_id, event_name), ())
        args_render = ", ".join(fmt_value(a) for a in args) if args else "<no args>"
        print(f"  {object_id}.{event_name} ({count} times); sample args: {args_render}")

except Exception as err:
    print(f"error: {err}")
    raise
