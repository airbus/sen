# sen::db

The Sen DB library provides the read/write API for Sen recordings. It is the engine behind the
[Recorder](../components/recording.md) and [Replayer](../components/replaying.md) components, and is
also directly accessible from C++ and Python for post-processing recorded data.

## What it stores

A recording is a directory on disk that contains:

| Item | Description |
|------|-------------|
| **Object data** | Timestamped property snapshots for every recorded object |
| **Keyframes** | Periodic full-state snapshots that enable random access |
| **Index** | A sorted index allowing O(log n) seeks to any point in time |
| **Annotations** | Free-form text labels attached to specific timestamps |
| **Snapshots** | On-demand full-state captures triggered at runtime |
| **Type metadata** | The type definitions needed to interpret the data without the original binaries |

All object data is compressed with [LZ4](https://en.wikipedia.org/wiki/LZ4_(compression_algorithm))
by default.

## Reading a recording

The `sen::db` API uses a cursor model: open the recording, seek to a position, and iterate forward
or backward. The replayer component wraps this with real-time clock synchronisation.

For scripted post-processing, Sen ships Python bindings that expose the same cursor API - see the
[Recorder example](../../examples/config/6_recorder/readme.md) for a working script.

## Writing a recording

Recordings are typically written by the Recorder component via the YAML configuration. Direct
writes through the `sen::db` API are possible for custom archiving scenarios.

## See Also

- [Recorder component](../components/recording.md)
- [Replayer component](../components/replaying.md)
- [Recorder example](../../examples/config/6_recorder/readme.md)
- [API Reference](../doxygen_gen/html/index.html) - full Doxygen documentation (available after building the docs)
