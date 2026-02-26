# Shapes example

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

The shape implementation you can find the `shapes.cpp` file defines the `update()` function. In it, we just
move it around, making it bounce in a box. The shape emits a `collidedWith(wall)` event when it collides with a wall.

The more interesting part is in `shape_listener.cpp`. Let's see how the `startListening` function is implemented:

```c++
  std::string startListeningToImpl(const std::string& bus,
                                   const MaybeColor& color,
                                   const MaybeInterval& xRange,
                                   const MaybeInterval& yRange) override
  {
    const auto query = makeQueryName();                                // create a name for our query
    auto sub = std::make_shared<sen::Subscription<ShapeInterface>>();  // create up a subscription
    sub->source = api_->getSource(bus);                                // get the bus

    // install the callbacks
    std::ignore = sub->list.onAdded([query, this](const auto& iterators) { shapesDetected(query, iterators); });
    std::ignore = sub->list.onRemoved([query, this](const auto& iterators) { shapesGone(query, iterators); });

    auto interest = makeInterest(query, bus, color, xRange, yRange);  // build the interest.
    sub->source->addSubscriber(interest, &sub->list, true);           // connect the list.
    subscriptions_.emplace(query, std::move(sub));                    // save the subscription.

    // update the query count
    setNextQueryCount(subscriptions_.size());
    return query;
  }
```

The `makeInterest()` function builds a Sen Query Language string adapted to the conditions defined by the user, for
example:

```sql
SELECT shapes.Shape FROM my.tutorial
 WHERE position.x BETWEEN 1.0 AND 2.0 AND
       position.y BETWEEN 0.0 AND 3.0 AND
       color IN (green, blue)
```

The listener prints the shapes that is able to detect.

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

```
sen run config/11_shapes/1_shapes_listener.yaml
```

In another terminal (let's call it B):

```
sen run config/11_shapes/1_shapes_producer.yaml
```

In another terminal (let's call it C):

```
sen shell
```

In terminal C, type:

```
open my.tutorial
my.tutorial.listener.startListeningTo "my.tutorial", null, null, null
```

In terminal `A` you will see:

- The query that was built for expressing the interest.
- The shapes that the listener is now detecting.
- Logs that are printed when the detected shapes emit some events.

If you now type the following in terminal C:

```
my.tutorial.listener.stopListening
```

In terminal `A` now you see that we don't get any new notification.

### Filtering by color

Now, try the same as before, but specify a color:

```
my.tutorial.listener.startListeningTo "my.tutorial", "green", null, null
```

### Filtering by location

```
my.tutorial.listener.startListeningTo "my.tutorial", null, {"min": 0, "max": 20}, null
```

In both dimensions:

```
my.tutorial.listener.startListeningTo "my.tutorial", null, {"min": 0, "max": 20}, {"min": 0, "max": 20}
```

### Filtering by all criteria

```
my.tutorial.listener.startListeningTo "my.tutorial", "green", {"min": 0, "max": 20}, {"min": 0, "max": 20}
```
