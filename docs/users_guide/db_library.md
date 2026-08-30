# sen::db

The Sen DB library provides the read/write API for Sen recordings. It is the engine behind the
[Recorder](../components/recording.md) and [Replayer](../components/replaying.md) components, and is
also directly accessible from C++ and Python for post-processing recorded data.

## What it stores

A recording is a directory on disk that contains:

| Item | Description |
|------|-------------|
| **Object data** | Timestamped property changes and events, plus the creation and deletion of each recorded object |
| **Keyframes** | Periodic full-state snapshots that enable random access |
| **Index** | A sorted index allowing O(log n) seeks to any point in time |
| **Annotations** | Free-form text labels attached to specific timestamps |
| **Snapshots** | The full state of one object at one time, which is what keyframes and creation entries are built from |
| **Type metadata** | The type definitions needed to interpret the data without the original binaries |

Keyframes are compressed with [LZ4](https://en.wikipedia.org/wiki/LZ4_(compression_algorithm)) by
default. The rest of the stream, meaning property changes, events, creations and deletions, is
written uncompressed, since each entry is small and the cost of compressing one would outweigh the
saving.

## Reading a recording

The `sen::db` API uses a cursor model: open the recording, position a cursor, and iterate forward.
Cursors only advance, so to revisit earlier data you open another one. `at(keyframeIndex)` starts
from a chosen keyframe, which is what the index is for, and `makeCursor(objectIndexDef)` walks a
single object's entries. The replayer component wraps this with real-time clock synchronization.

For scripted post-processing, Sen ships [Python bindings](db_python_bindings.md) that expose the
same cursor API.

## Writing a recording

Recordings are typically written by the recorder component via the YAML configuration. Direct
writes through the `sen::db` API are possible for custom archiving scenarios.

## See Also

- [Recorder component](../components/recording.md)
- [Replayer component](../components/replaying.md)
- [Recorder example](../snippets/examples/config/6_recorder/readme.md)
- [API Reference](../doxygen_gen/html/index.html): full Doxygen documentation (available after
  building the docs)
