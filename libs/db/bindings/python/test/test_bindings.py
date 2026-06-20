# === test_bindings.py =================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Smoke tests for the sen_db_python bindings.

Walks `components/recorder/test/data/crash_recording` (path passed as argv[1] by CMake) and
asserts the bindings' documented surface. The fixture is intentionally truncated (the writer
was SIGKILL'd to test crash resilience) but the readable portion is a valid recording: see
components/recorder/test/recorder_crash_test.cpp's RealCrashedRecording_CanBeOpenedAndReadPartially,
which walks the same fixture via sen::db::Input and asserts speed changes 250 -> 123. The cursor
stops cleanly at the truncation point.

Coverage: Summary, getTypes() + TypeRegistryPy (classNames / __len__ / __contains__ /
getTypeSpec), DataCursor walk + isinstance dispatch on every payload arm, Snapshot reflection
(objectId / className / propertyNames + getattr fall-through + AttributeError), KeyedVar
dict-shape contract, lifetime safety (held SnapshotList survives advance()).

Standalone -- no pytest dependency. Exit code 0 on success.
"""

import sys

import sen_db_python as sen


def expect_attribute_error(callable_, name):
    """Assert that calling `callable_` raises AttributeError; otherwise fail with `name` in the message."""
    try:
        callable_()
    except AttributeError:
        return
    raise AssertionError(f"expected AttributeError for {name}, got success")


def check_summary(summary):
    """Summary surface: every advertised field exists and has the documented kind."""
    assert isinstance(summary.objectCount, int)
    assert isinstance(summary.typeCount, int)
    assert isinstance(summary.keyframeCount, int)
    assert isinstance(summary.annotationCount, int)
    assert isinstance(summary.indexedObjectCount, int)
    assert summary.firstTime <= summary.lastTime
    assert summary.objectCount > 0
    assert summary.keyframeCount > 0


def check_type_registry(registry):
    """TypeRegistryPy surface: __len__, __contains__, classNames, getTypeSpec."""
    n = len(registry)
    assert n > 0
    class_names = registry.classNames
    assert isinstance(class_names, list)
    assert len(class_names) == n
    assert all(isinstance(name, str) for name in class_names)
    sample = class_names[0]
    assert sample in registry
    assert "definitely_not_a_real_type" not in registry
    spec = registry.getTypeSpec(sample)
    assert spec is None or isinstance(spec, dict)
    assert registry.getTypeSpec("definitely_not_a_real_type") is None


def check_snapshot_surface(snapshot):
    """Snapshot reflection: built-in attrs + every advertised property name resolves; unknown raises."""
    assert isinstance(snapshot.objectId, int)
    assert isinstance(snapshot.className, str) and snapshot.className
    names = snapshot.propertyNames
    assert isinstance(names, list)
    # propertyNames must be stable across reads (cache from m16).
    assert snapshot.propertyNames == names
    for name in names:
        # getattr fall-through resolves real property names without raising.
        value = getattr(snapshot, name)
        check_value_shape(value)
    expect_attribute_error(
        lambda: snapshot.definitely_not_a_property,
        "snapshot.definitely_not_a_property",
    )


def check_value_shape(value):
    """A converted Python value is one of: None, scalar, list, or dict (struct or KeyedVar)."""
    if value is None or isinstance(value, (bool, int, float, str)):
        return
    if isinstance(value, list):
        for elem in value:
            check_value_shape(elem)
        return
    if isinstance(value, dict):
        if set(value.keys()) == {"type", "value"}:
            # KeyedVar shape: tag is a qualified type-name string, value follows the arm.
            assert isinstance(value["type"], str)
            check_value_shape(value["value"])
            return
        # Plain struct / VarMap.
        for k, v in value.items():
            assert isinstance(k, str)
            check_value_shape(v)
        return
    raise AssertionError(f"unexpected value kind: {type(value).__name__}")


def main(recording_path):  # noqa: C901  -- test orchestrator naturally fans out per payload kind
    """Walk the recording at `recording_path` and assert the bindings' documented surface."""
    src = sen.Input(recording_path)
    check_summary(src.summary)
    check_type_registry(src.getTypes())
    # getTypes() is cached on InputPy (m12); second call returns the same wrapper.
    assert src.getTypes() is src.getTypes()

    # Walk the cursor; count payload kinds via isinstance dispatch, collect speed values, and
    # exercise the lifetime guarantee that held SnapshotLists survive advance() (M5 fix).
    counts = {"Keyframe": 0, "PropertyChange": 0, "Event": 0, "Creation": 0, "Deletion": 0, "End": 0}
    saw_snapshot = False
    speed_changes = []
    held_snapshots = None  # captured from an early Keyframe; walked after advance() further along
    cursor = src.begin()
    # The cursor starts in a sentinel ("at begin") state -- advance once before reading,
    # mirroring the `++cursor; if (cursor.atEnd()) break;` pattern from the C++ test.
    while True:
        cursor.advance()
        if cursor.atEnd:
            break
        entry = cursor.entry
        payload = entry.payload
        if isinstance(payload, sen.Keyframe):
            counts["Keyframe"] += 1
            for snapshot in payload.snapshots:
                saw_snapshot = True
                check_snapshot_surface(snapshot)
            if held_snapshots is None:
                held_snapshots = payload.snapshots
        elif isinstance(payload, sen.PropertyChange):
            counts["PropertyChange"] += 1
            assert isinstance(payload.objectId, int)
            assert isinstance(payload.name, str) and payload.name
            value = payload.value
            check_value_shape(value)
            if payload.name == "speed":
                speed_changes.append(value)
        elif isinstance(payload, sen.Event):
            counts["Event"] += 1
            assert isinstance(payload.objectId, int)
            assert isinstance(payload.name, str) and payload.name
            assert isinstance(payload.args, list)
            for arg in payload.args:
                check_value_shape(arg)
        elif isinstance(payload, sen.Creation):
            counts["Creation"] += 1
            # Creation forwards Snapshot reflection via __getattr__; objectId/className must work.
            assert isinstance(payload.objectId, int)
            assert isinstance(payload.className, str)
        elif isinstance(payload, sen.Deletion):
            counts["Deletion"] += 1
            assert isinstance(payload.objectId, int)
        elif isinstance(payload, sen.End):
            counts["End"] += 1
        else:
            raise AssertionError(f"unexpected payload kind: {type(payload).__name__}")

    # Lifetime guarantee from [06/5] M5: payload copy + keep_alive<0,1> keeps the held
    # SnapshotList valid even after the cursor advanced past its Keyframe.
    assert held_snapshots is not None, "expected at least one Keyframe in the walk"
    for snapshot in held_snapshots:
        # Should not segfault or AttributeError -- the Snapshot's storage is alive via keep_alive.
        assert isinstance(snapshot.className, str)

    assert saw_snapshot, "walk should surface at least one Snapshot via Keyframe"
    assert len(speed_changes) >= 2, f"expected >= 2 speed changes, got {len(speed_changes)}"
    assert speed_changes[0] == 250.0, f"first speed change should be 250, got {speed_changes[0]}"
    assert speed_changes[-1] == 123.0, f"last speed change should be 123, got {speed_changes[-1]}"

    print(f"OK: counts={counts}, speed_changes={len(speed_changes)}, types={len(src.getTypes())}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <recording-path>", file=sys.stderr)
        sys.exit(2)
    main(sys.argv[1])
