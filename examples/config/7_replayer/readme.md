# Replayer example

This example uses the recordings produced by the previous example.

## How to run it

```
sen run config/7_replayer/1_replayer_school.yaml
```

This will automatically start and recreate the objects that were recorded. You can inspect them
using the shell, open a `sen explorer`, etc.

You can also interact with the replayer API by running:

```
sen run config/7_replayer/2_replayer_alone.yaml
```

Then you can run commands such as:

```
local.replay.replayer.print

local.replay.replayer.open "my_replay", "school_recording"
local.replay.my_replay.print
```
