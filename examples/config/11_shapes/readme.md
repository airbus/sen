# Shapes example

> **Prerequisites:** [4 - School](../4_school/readme.md) (object discovery, subscriptions).

This example illustrates how to manage interests on objects using C++.

The idea is:

- You publish some shapes on a Sen bus.
- Shapes move around, emit events and change color.
- In another process, you instantiate a `ShapeListener`.
- Using a Sen shell, you tell the listener to start/stop listening to shapes using some criteria.
- You will see how the listener is automatically informed by Sen on the objects that match the given
  criteria.

## Interface

This is the data model:

=== "_shapes.stl_"

    ```rust
    --8<-- "snippets/examples/packages/shapes/stl/shapes.stl"
    ```

=== "_shape_listener.stl_"

    ```rust
    --8<-- "snippets/examples/packages/shapes/stl/shape_listener.stl"
    ```

## Implementation

The shape implementation you can find the `shapes.cpp` file defines the `update()` function. In it,
we just move it around, making it bounce in a box. The shape emits a `collidedWithWall(wall)` event
when it collides with a wall.

The more interesting part is in `shape_listener.cpp`. Let's see how the `startListening` function is
implemented:

```c++
--8<-- "snippets/examples/packages/shapes/src/shape_listener.cpp:start_listening"
```

The `buildQuery()` function builds a Sen Query Language string adapted to the conditions defined by
the user, for example:

```sql
SELECT shapes.Shape FROM my.tutorial
 WHERE position.x BETWEEN 1.0 AND 2.0 AND
       position.y BETWEEN 0.0 AND 3.0 AND
       color IN (green, blue)
```

The listener prints the shapes that is able to detect.

## Subscription lifecycle

The ShapeListener manages two independent things, created at different moments and torn down
separately.

**The subscription to a bus, one per query.** `api_->selectFrom<ShapeInterface>(bus, query, onAdded,
onRemoved)` returns a `std::shared_ptr<Subscription<ShapeInterface>>` holding an `ObjectList`, with
both callbacks already installed so that they also fire for shapes that were already there. The
listener keeps it in `subscriptions_`, keyed by query name. Erasing that entry destroys the
`Subscription`, which unregisters the list from the bus. Nothing else has to be kept alive:
`onAdded` and `onRemoved` are plain callbacks, and installing one replaces whatever was there
before.

**The collision callback on each discovered shape, one per shape.** `shape->onCollidedWithWall(...)`
returns a `ConnectionGuard`, which unregisters the callback when it is destroyed. These do have to
be kept alive, so the listener stores them in `shapeGuards_`, keyed by object id. Erasing a shape's
entry drops its guards and unregisters its callbacks.

## How to run it

Let's define what we want to run in our Sen kernel.

```yaml title="Producer configuration"
--8<-- "snippets/examples/config/11_shapes/1_shapes_producer.yaml"
```

```yaml title="Listener configuration"
--8<-- "snippets/examples/config/11_shapes/1_shapes_listener.yaml"
```

### Listening to all shapes

In one terminal (let's call it A):

```shell
sen run config/11_shapes/1_shapes_listener.yaml
```

In another terminal (let's call it B):

```shell
sen run config/11_shapes/1_shapes_producer.yaml
```

In another terminal (let's call it C):

```shell
sen shell
```

In terminal C, type:

```text
open my.tutorial
my.tutorial.listener.startListeningTo "my.tutorial", null, null, null
```

In terminal `A` you will see:

- The query that was built for expressing the interest.
- The shapes that the listener is now detecting.
- Logs that are printed when the detected shapes emit some events.

If you now type the following in terminal C:

```text
my.tutorial.listener.stopListening
```

In terminal `A` now you see that we don't get any new notification.

### Filtering by color

Now, try the same as before, but specify a color:

```text
my.tutorial.listener.startListeningTo "my.tutorial", "green", null, null
```

### Filtering by location

```text
my.tutorial.listener.startListeningTo "my.tutorial", null, {"min": 0, "max": 20}, null
```

In both dimensions:

```text
my.tutorial.listener.startListeningTo "my.tutorial", null, {"min": 0, "max": 20}, {"min": 0, "max": 20}
```

### Filtering by all criteria

```text
my.tutorial.listener.startListeningTo "my.tutorial", "green", {"min": 0, "max": 20}, {"min": 0, "max": 20}
```

## Using the explorer

You can replace the producer config with the explorer variant to visually monitor shape positions,
property changes, and collision events in real time:

In terminal A (listener, unchanged):

```shell
sen run config/11_shapes/1_shapes_listener.yaml
```

In terminal B (producer with explorer GUI):

```shell
sen run config/11_shapes/2_shapes_producer_exp.yaml
```

Then use the explorer to open the `my.tutorial` bus and monitor the shapes.
