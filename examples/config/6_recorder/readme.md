# Recorder example

> **Prerequisites:** [4 - School](../4_school/readme.md) (produces the objects we'll record).

This example shows how to use the Sen recorder component.

## Recording some local objects

Here we just set it up to record some objects that are always published by Sen. They live in the
`local.kernel` bus.

```
sen run config/6_recorder/1_recorder_kernel.yaml
```

This will automatically start the recording. You can inspect the objects (including the recording).

After a while you can close the process (with a `shutdown` command or a `Ctrl+D` keystroke).

The recording should be present in a new folder called `kernel_recording`. To inspect it:

```
sen archive info kernel_recording
```

You should see something similar to this:

```
  path:            kernel_recording
  duration:        8.06667s
  start:           2025-05-19 14:34:45 966836
  end:             2025-05-19 14:34:54 033502
  objects:         4
  types:           30
  annotations:     0
  keyframes:       5
  indexed objects: 4
```

## Recording the school example

Similar to the previous example, but now we also start the school example and record more
information (don't run it for too long, as we will be iterating over the entries later on).

```
sen run config/6_recorder/2_recorder_school.yaml
```

Once closed, we want to explore the data using a Python script.

Ensure you have your Python path configured:

For bash:

```
export PYTHONPATH=$PYTHONPATH;$SEN_PATH/bin
```

For fish:

```
set -xa PYTHONPATH $SEN_PATH/bin
```

Then, run:

```
python3 config/6_recorder/3_recorder_school_print.py
```

## Using Python to inspect the recordings

Sen comes with a Python binding to access the recorded data. The following Python code prints the entries:

```python
--8<-- "snippets/examples/config/6_recorder/3_recorder_school_print.py"
```

## Using C++ to inspect the recordings

You can access the recorded data using the provided C++ API (this is how the Sen replayer works):

```c++
--8<-- "snippets/examples/apps/recording_inspector/main.cpp"
```
