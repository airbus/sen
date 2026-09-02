# sen::db Python bindings

The `sen_db_python` module exposes the [sen::db](db_library.md) cursor API to Python.
Scripts can open a recording, walk its contents, and inspect every value, type, snapshot,
and annotation without the original C++ binaries.

## Importing

```python
import sen_db_python as sen
```

The conventional alias is `sen`. The rest of this document assumes it.

## Opening a recording

```python
inp = sen.Input("/path/to/recording_directory")
print(inp.path)        # str -- the recording directory
summary = inp.summary  # Summary object
```

`Summary` carries:

| Attribute             | Type                  | Meaning                                |
| --------------------- | --------------------- | -------------------------------------- |
| `firstTime`           | `datetime.timedelta`  | time of the first sample, since epoch  |
| `lastTime`            | `datetime.timedelta`  | time of the last sample, since epoch   |
| `keyframeCount`       | `int`                 | number of keyframes                    |
| `objectCount`         | `int`                 | number of distinct objects             |
| `typeCount`           | `int`                 | number of distinct types               |
| `annotationCount`     | `int`                 | number of annotations                  |
| `indexedObjectCount`  | `int`                 | number of objects with random access   |

## The cursor model

A `DataCursor` iterates over the runtime data. Three constructors:

| API                                | Cursor over                                  |
| ---------------------------------- | -------------------------------------------- |
| `inp.begin()`                      | the whole recording, from the start          |
| `inp.at(keyframeIndex)`            | the whole recording, from a chosen keyframe  |
| `inp.makeCursor(objectIndexDef)`   | a single object's entries only               |

Walk pattern:

```python
cursor = inp.begin()
cursor.advance()              # move to the first entry
while not cursor.atEnd:
    entry = cursor.entry
    # entry.time     -> datetime.timedelta since epoch
    # entry.payload  -> one of PropertyChange / Event / Keyframe /
    #                   Creation / Deletion / End
    ...
    cursor.advance()
```

`cursor.atStart` is `True` before any `advance()`; in that state `entry.payload` is `None`.
`cursor.atEnd` is `True` once the cursor reaches the recording's end.

The payload accessor returns the active variant alternative directly, so `isinstance()`
dispatches against concrete classes:

```python
if isinstance(entry.payload, sen.PropertyChange):
    ...
elif isinstance(entry.payload, sen.Event):
    ...
```

## Payload classes

### `PropertyChange`

| Attribute    | Type                       |
| ------------ | -------------------------- |
| `.objectId`  | `int`                      |
| `.name`      | `str` -- property name     |
| `.value`     | converted Python value     |

### `Event`

| Attribute    | Type                                   |
| ------------ | -------------------------------------- |
| `.objectId`  | `int`                                  |
| `.name`      | `str` -- event name                    |
| `.args`      | `list[object]` -- one converted value per argument |

### `Keyframe`

| Attribute     | Type                              |
| ------------- | --------------------------------- |
| `.snapshots`  | sequence of `Snapshot`            |

### `Creation`

Delegates attribute lookup to the embedded `Snapshot`. Read object fields directly:
`creation.name`, `creation.className`, `creation.<propertyName>`, etc. See
[Snapshot reflection](#snapshot-reflection).

### `Deletion`

| Attribute    | Type    |
| ------------ | ------- |
| `.objectId`  | `int`   |

### `End`

Sentinel marking the end of the cursor. The idiomatic check is `cursor.atEnd`.

## Snapshot reflection

`Snapshot` exposes attributes via reflection against the object's class. Built-in
attributes return identity and class metadata; any other name resolves as a property
defined on the class (walking parents).

| Attribute          | Returns                                            |
| ------------------ | -------------------------------------------------- |
| `.name`            | `str` -- object name                               |
| `.busName`         | `str`                                              |
| `.sessionName`     | `str`                                              |
| `.objectId`        | `int` -- object id                                 |
| `.className`       | `str` -- qualified class name                      |
| `.propertyNames`   | `list[str]` -- every property, including inherited |
| `.<propertyName>`  | the property's converted Python value              |

Reading an attribute that is neither built-in nor a known property raises
`AttributeError`.

```python
snap = keyframe.snapshots[0]
print(snap.className)              # 'school.Student'
print(snap.propertyNames)          # ['firstName', 'surName', 'focusLevel', ...]
print(snap.focusLevel)             # 0.42
```

## Value conversion

Sen values are converted to native Python equivalents. The mapping is uniform across
`PropertyChange.value`, `Event.args`, `Annotation.value`, and every
`Snapshot.<propertyName>` accessor.

| Sen value                               | Python value                                   |
| --------------------------------------- | ---------------------------------------------- |
| `bool`, integers, floats                | matching Python builtin                        |
| `string`                                | `str`                                          |
| `TimeStamp`, `Duration`                 | `datetime.timedelta`                           |
| `VarList` (sequence)                    | `list`                                         |
| `VarMap` (named fields)                 | `dict`                                         |
| `KeyedVar` (tagged variant alternative) | `{"type": "<qualifiedName>", "value": <converted>}` -- same shape as the type registry uses |
| structs                                 | `dict` (via `VarMap`)                          |

## Type registry

`inp.getTypes()` returns a `TypeRegistry` snapshot describing every class, struct,
variant, alias, enum, and quantity present in the recording.

| API                                  | Returns                                                                |
| ------------------------------------ | ---------------------------------------------------------------------- |
| `len(registry)`                      | `int` -- number of registered types                                    |
| `name in registry`                   | `bool`                                                                 |
| `.classNames`                        | `list[str]` -- every qualified name                                    |
| `.getTypeSpec(qualifiedName)`        | `dict` describing the type, or `None` if absent                        |
| `.getAllTypeSpecs()`                 | `dict[str, dict]` -- every type, keyed by qualified name               |

The dict shape is the kernel's external `CustomTypeSpec` encoding. Top level:

| Key             | Type                          | Meaning                                  |
| --------------- | ----------------------------- | ---------------------------------------- |
| `qualifiedName` | `str`                         | fully qualified type name                |
| `name`          | `str`                         | short name (without the package prefix)  |
| `description`   | `str`                         | description from the STL definition      |
| `data`          | `{type: str, value: dict}`    | tagged union -- shape depends on `type`  |

`data.type` is one of:

| Tag                              | `data.value` shape                                                                                |
| -------------------------------- | ------------------------------------------------------------------------------------------------- |
| `sen.kernel.ClassTypeSpec`       | `{properties, methods, events, constructor, parents, isInterface}`                                |
| `sen.kernel.StructTypeSpec`      | `{fields, parent}` -- each field has `name`, `description`, `type`; `parent` empty for none       |
| `sen.kernel.VariantTypeSpec`     | `{fields}` -- each field has `key`, `description`, `type`                                         |
| `sen.kernel.EnumTypeSpec`        | `{enums, storageType}` -- each enum has `name`, `key`, `description`                              |
| `sen.kernel.AliasTypeSpec`       | `{aliasedType}`                                                                                   |
| `sen.kernel.QuantityTypeSpec`    | `{elementType, unit, minValue, maxValue}`                                                         |
| `sen.kernel.SequenceTypeSpec`    | `{elementType, maxSize, fixedSize}`                                                               |
| `sen.kernel.OptionalTypeSpec`    | `{type}`                                                                                          |

`ClassTypeSpec` members:

- **properties[i]**: `name`, `description`, `category` (`staticRO` / `staticRW` / `dynamicRO` /
  `dynamicRW`), `type`, `transportMode`, `tags`, `checkedSet`.
- **methods[i]**: `name`, `description`, `args` (list of `{name, description, type}`),
  `transportMode`, `constness`, `deferred`, `returnType`, `propertyRelation`, `localOnly`.
- **events[i]**: `name`, `description`, `args` (same shape as method args), `transportMode`.
- **constructor**: same shape as `methods[i]`.
- **parents**: list of qualified parent class names.
- **isInterface**: `bool`.

Inheritance is walked via `parents`. To find an event defined on an ancestor:

```python
def find_event(reg, class_name, event_name):
    spec = reg.getTypeSpec(class_name)
    if spec is None:
        return None
    data = spec["data"]["value"]
    for event in data.get("events", []):
        if event["name"] == event_name:
            return event
    for parent in data.get("parents", []):
        found = find_event(reg, parent, event_name)
        if found is not None:
            return found
    return None
```

## Indexes

| API                                       | Returns                                                                |
| ----------------------------------------- | ---------------------------------------------------------------------- |
| `inp.getObjectIndexDefinitions()`         | sequence of `ObjectIndexDef`                                           |
| `inp.getAllKeyframeIndexes()`             | sequence of `KeyframeIndex`                                            |
| `inp.getKeyframeIndex(time)`              | `KeyframeIndex` or `None` -- nearest keyframe in either direction; `None` only when there are no keyframes at all |
| `inp.at(keyframeIndex)`                   | new `DataCursor` starting at that keyframe                             |
| `inp.makeCursor(objectIndexDef)`          | new `DataCursor` iterating one object only                             |

`ObjectIndexDef` carries `objectId`, `name`, `session`, `bus`, `indexId`, and `type`
(qualified class name). `KeyframeIndex` carries `offset` and `time`.

All three accessors read the same indexes file on first call. If the recording was written
with a writer version whose index format the current reader doesn't support -- or the file
is otherwise malformed -- the first call (whichever of the three it is) raises; subsequent
calls re-raise. The same per-object metadata is reachable by walking the cursor and
collecting `Creation` payloads:

```python
inp = sen.Input("/path/to/recording")
cursor = inp.begin()
cursor.advance()
objects = {}
while not cursor.atEnd:
    p = cursor.entry.payload
    if isinstance(p, sen.Creation):
        objects[p.objectId] = {"name": p.name, "className": p.className, "bus": p.busName}
    cursor.advance()
```

## Annotations

`inp.annotationsBegin()` returns an `AnnotationCursor` with the same shape as
`DataCursor` -- `atStart`, `atEnd`, `entry`, `advance()`. The payload is either
`Annotation` or `End`.

| `Annotation` attribute | Type                                                                       |
| ---------------------- | -------------------------------------------------------------------------- |
| `.type`                | `str` -- qualified name for custom types; basic type name (e.g. `f64`) for built-ins |
| `.value`               | converted Python value                                                     |

## Time

All times are `datetime.timedelta` since the Unix epoch. Convert to a wall-clock
`datetime`:

```python
from datetime import datetime

EPOCH = datetime(1970, 1, 1)
wall_clock = EPOCH + entry.time
```

## Output discipline

Calling code (CLI / [MCP gateway](../components/mcp_gateway.md) / etc.) typically caps script
output. The gateway defaults to 64 KiB of stdout, 16 KiB of stderr, and 60 s of wall clock, the last
raisable to 600 s through `SEN_RECORDING_TIMEOUT_MS`. It also caps the script itself
at 64 KiB. That limit matches the pipe buffer, so a larger script stalls on the write
rather than failing with a message. Print
*summaries* (not raw rows), and bound the walk by time-window or sample count when the
recording is large.

### Counts by category

```python
from collections import defaultdict
counts = defaultdict(int)
# ... walk ...
counts[key] += 1
print(dict(counts))
```

### Per-key stats

```python
samples = defaultdict(list)
# ... walk: samples[key].append(value) ...
for key, values in samples.items():
    print(f"{key}: n={len(values)} min={min(values):.3f} "
          f"mean={sum(values)/len(values):.3f} max={max(values):.3f}")
```

### Top-K

<!-- pyml disable-num-lines 5 no-reversed-links -->
```python
top = sorted(counts.items(), key=lambda kv: -kv[1])[:10]
for key, count in top:
    print(f"{key}: {count}")
```

### Time-window bins

`entry.time` is a `datetime.timedelta`; dividing two `timedelta`s gives a float, so
binning is straightforward:

```python
from datetime import timedelta
window = timedelta(seconds=5)
bins = defaultdict(int)
# ... walk: bins[int(entry.time / window)] += 1 ...
for idx in sorted(bins):
    print(f"t={idx*5:>4}s n={bins[idx]}")
```

### Sampling, not enumeration

When showing examples, cap how many you keep:

```python
EXAMPLES_PER_KEY = 3
examples = defaultdict(list)
# ... walk: keep only the first few per key ...
if len(examples[key]) < EXAMPLES_PER_KEY:
    examples[key].append(value)
```

## End-to-end example

Compute per-student focus statistics and rank events by frequency across a school
recording, in a single cursor walk:

```python
import sen_db_python as sen
from collections import defaultdict
from datetime import datetime

EPOCH = datetime(1970, 1, 1)
inp = sen.Input("/data/recordings/school_run_42")

# 1. Sanity-check the schema via the type registry.
reg = inp.getTypes()
student_spec = reg.getTypeSpec("school.Student")
assert student_spec is not None
property_names = [p["name"] for p in student_spec["data"]["value"]["properties"]]
assert "focusLevel" in property_names

# 2. Walk the recording once, collecting samples per student and counting events.
samples = defaultdict(list)
names_by_id = {}
event_counts = defaultdict(int)

cursor = inp.begin()
cursor.advance()
while not cursor.atEnd:
    p = cursor.entry.payload
    if isinstance(p, sen.Creation):
        names_by_id[p.objectId] = p.name
    elif isinstance(p, sen.PropertyChange) and p.name == "focusLevel":
        samples[p.objectId].append(p.value)
    elif isinstance(p, sen.Event):
        event_counts[p.name] += 1
    cursor.advance()

# 3. Aggregate and report.
print(f"Window: {EPOCH + inp.summary.firstTime} .. {EPOCH + inp.summary.lastTime}")

print("\nFocus statistics:")
for object_id, values in samples.items():
    name = names_by_id.get(object_id, f"<id {object_id}>")
    mean = sum(values) / len(values)
    print(f"  {name:<20} n={len(values):>4} mean={mean:.3f}")

print("\nEvent counts:")
for name, count in sorted(event_counts.items(), key=lambda kv: -kv[1]):
    print(f"  {name:<24} {count}")
```

## See Also

- [sen::db](db_library.md)
- [Recorder component](../components/recording.md)
- [Replayer component](../components/replaying.md)
- [Recorder example](../snippets/examples/config/6_recorder/readme.md)
