# Aircraft example

Here you can see how to use an HLA-based definition to implement a simple aircraft simulation using
the `update()` method.

## Interface and Implementation

In the `aircraft.stl` file you can see how we inherit from `rpr.Aircraft`, and in the implementation
`dummy_aircraft.cpp` you can see a (dummy) implementation of the `update` function.

=== "_dummy_aircraft.stl_"

    ```rust
    --8<-- "snippets/examples/packages/aircrafts/stl/dummy_aircraft.stl"
    ```

=== "_dummy_aircraft_impl.cpp_"

    ```cpp
    --8<-- "snippets/examples/packages/aircrafts/src/dummy_aircraft_impl.cpp"
    ```

## How to run it

Let's define what we want to run in our Sen kernel.

```yaml title="Configuration file"
--8<-- "snippets/examples/config/3_aircraft/1_aircraft.yaml"
```

This will open a shell and tell Sen to instantiate one aircraft in the `my.tutorial` bus.

To run it, let's call `sen run`:

```
sen run config/3_aircraft/1_aircraft.yaml
```

You should be able to repeatedly call `my.tutorial.myAircraft.getSpatial` and see the evolution of
the position.

## Using the explorer

You can also load the explorer to have an easier time following the updates.

```
sen run config/3_aircraft/2_aircraft_exp.yaml
```

## Using virtualized time

Let's now start the kernel in virtualized time mode by changing our config file.

```yaml title="Configuration file"
--8<-- "snippets/examples/config/3_aircraft/3_aircraft_virtual_time.yaml"
```

We can run it with the following command:
```
sen run config/3_aircraft/3_aircraft_virtual_time.yaml
```

Now, if you execute `my.tutorial.myAircraft.getSpatial` you will see that there are no updates.

Let's now advance one cycle by using the clock API.

```
my.tutorial.master.step
my.tutorial.myAircraft.getSpatial
```

You can see how the component got triggered.

If you want to advance the time more, say 60 seconds:

```
my.tutorial.master.advanceTime "60 s"
```

This should run relatively fast, as we are not performing expensive computations.
