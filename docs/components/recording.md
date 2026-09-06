# Recording

![Screenshot](../assets/images/recorder_light.svg#only-light){: style="width:300px; float: right;"}
![Screenshot](../assets/images/recorder_dark.svg#only-dark){: style="width:300px; float: right;"}

The recorder component allows you to record the execution of your system. It is highly optimized,
and is designed to co-exist with other tools that allow you to access, modify and replay the stored
data.

## Basic idea

This component is able to:

- Record the properties of any object in any bus.
- Record the events emitted by any object.
- Create "keyframes": periodic full-state snapshots that let a replay start from any point in the
  recording instead of from the beginning. They restore state into a replay, not into a live system;
  Sen has no equivalent of a federation save/restore.
- Create "indexes" for the keyframes and for selected objects. Everything an indexed object
  produces goes into its index, so you can jump straight to it in post-processing.
- Automatically capture all the meta-data. This includes all the types and documentation of the
  recorded objects.
- Use the recorded meta-data to allow for backwards-compatible replays.
- Add arbitrary annotations to the recording, without altering the existing content.

An archive holds what objects published: property changes, events, creations and deletions, plus the
keyframes, annotations and snapshots you asked for. It does not hold method calls: a call from one
component to another is not an entry type the archive has.

Coverage is whatever your `selections` matched. A bus nobody selected is not in the archive, so
treat the selections as the definition of what you will be able to review later.

The serialization is binary. Keyframes are compressed with LZ4; property changes, events, creations
and deletions are written uncompressed. Also, the threading model is designed to parallelize the
work and minimize any overhead to other components running in the same process.

## Recording your system

The recorder component can be started in an independent process, or together with some running
components.

When deployed stand-alone, you will be able to record whatever gets published over the buses. There
might be network traffic, as the information needs to reach the process so that it can record it. An
advantage of this approach is that you don't need to touch an existing system to perform the
recording. You simply start it when needed.

When deployed with some other components, you will be able to also record local objects. If those
objects produce a lot of data, you will avoid the network traffic.

To load the recorder component, you just need to specify it in the configuration file:

```yaml title="Recording the objects in local.kernel"
load:
  - name: recorder  # load the recorder component
    group: 2
    recordings:
      - name: kernel_recording
        folder: .
        indexKeyframes: true
        indexObjects: true
        keyframePeriod: 2 s
        autoStart: true
        selections:
          - SELECT * FROM local.kernel  # record all objects in local.kernel
```

The configuration options are defined in the component's STL:

```rust title="Recorder configuration options"
--8<-- "snippets/recorder_config.stl"
```

You can create multiple recordings (each recording generates an archive), and define multiple
selection criteria for each recording. You can use those to define the objects to be tracked and
recorded.

Loading the recorder as a component, as above, is the convenient route: it runs on its own cycle and
records what it sees when it drains, so a value that changes twice between those cycles reaches the
archive once. `sen.components.recorder` is also an ordinary package, so you can instantiate a
`Recorder` object alongside the objects you want to record; it then runs inside their component's
cycle and misses nothing.

You can also have more control over the recording state by using the `Recorder` object that this
component publishes. Those objects offer an interface to control and monitor their execution:

```rust title="Recorder interface"
--8<-- "snippets/recorder.stl"
```

## Inspecting the recorded data

Recordings are stored in their own folder. Those folders contain multiple files that store different
things. You can inspect the contents of those files by using the command-line tool:

```title="sen archive"
--8<-- "snippets/sen_archive.sh"
```

### Basic information

```title="sen archive info"
--8<-- "snippets/sen_archive_info.sh"
```

### Indexed objects

```title="sen archive indexed"
--8<-- "snippets/sen_archive_indexed.sh"
```

## Accessing the data with the C++ API

The API for manipulating recordings is part of the Sen library. It is in the `db` folder.

The main entry point for reading the recordings is the `Input` class. You can use it to open an
archive, inspect general information, create cursors to iterate over the entries, and create
annotations.

Example:

```c++ title="Using the Input class and a cursor to iterate over the data"
sen::CustomTypeRegistry nativeTypes;
sen::db::Input input(archivePath, nativeTypes);

const auto& summary = input.getSummary();

std::cout << "  path:            " << archivePath.string() << "\n";
std::cout << "  duration:        " << toSeconds(summary.lastTime - summary.firstTime) << "s\n";
std::cout << "  start:           " << summary.firstTime.toLocalString() << "\n";
std::cout << "  end:             " << summary.lastTime.toLocalString() << "\n";
std::cout << "  objects:         " << summary.objectCount << "\n";
std::cout << "  types:           " << summary.typeCount << "\n";
std::cout << "  annotations:     " << summary.annotationCount << "\n";
std::cout << "  keyframes:       " << summary.keyframeCount << "\n";
std::cout << "  indexed objects: " << summary.indexedObjectCount << "\n";

// iterate over all the entries (the cursor starts before the first one)
auto cursor = input.begin();
for(++cursor; !cursor.atEnd(); ++cursor)
{
    std::cout << " time: " << cursor.get().time.toLocalString() << "\n";

    // use the data of the entry
    std::visit(sen::Overloaded {[](const PropertyChange& /*val*/) { },
                                [](const Event& /*val*/) { },
                                [](const Keyframe& /*val*/) { },
                                [](const Creation& /*val*/) { },
                                [](const Deletion& /*val*/) { },
                                [](const auto& /* nothing */) { }},
                    cursor.get().payload);
}
```

## Accessing the data with the Python API

Sen includes a Python module so that you can use Python for data processing.

```Python title="Using the Python API to read an archive"
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
```
