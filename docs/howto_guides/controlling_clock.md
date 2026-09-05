# Controlling the clock

The kernel can publish a clock object that lets you control the advance of virtual time.

`runMode: virtualTime` means the components advance only when something steps them, so a kernel
started that way and left alone sits there doing nothing. That is the mode this page is about. If
what you want instead is virtual time that runs on its own, as fast as the components allow, use
`runMode: virtualTimeRunning`: the kernel advances the clock itself and you still get virtual time
rather than the system clock. The [Execution model](../users_guide/execution_model.md) lists all
four run modes.

You can enable it by adding this to your configuration file:

```yaml
kernel:
  runMode: virtualTime
  clockMaster: true

# ... rest of the configuration file
```

This will make the kernel publish an object called "master" in a bus. That bus is `clockBus`, which
defaults to the kernel's own `bus`, which in turn defaults to `local.kernel`. You can point the
clock somewhere else by setting `clockBus`, for example:

```yaml
kernel:
  runMode: virtualTime
  clockMaster: true
  clockBus: se.clocks

# ... rest of the configuration file
```

The object you get is a `VirtualMasterClock`. There are three clock classes, and it helps to see
them together:

```rust title="kernel_objects.stl"
--8<-- "libs/kernel/stl/sen/kernel/kernel_objects.stl:clocks"
```

`VirtualClock` is the part you read: `time` is the current virtual time. Every kernel publishes a
`VirtualKernelClock` for the components it hosts, and the master discovers those and drives them.
The one you write to is `delta` on the master, which is the size of a single step, not the current
time.

## Stepping the time

1. Start your kernel in virtualized time mode.
2. Tell it to instantiate a clock by setting `clockMaster` to `true`.
3. Find the "master" object and set a value for `delta`, the size of one step.
4. Call `step()` or `steps(n)` or `advanceTime(duration)`.

For example, here we run the "school" example and use the following kernel config:

```yaml
kernel:
  runMode: virtualTime
  clockMaster: true
```

We use the kernel clock to step the time and monitor one object in the
[explorer](../components/explorer.md), plotting one of its properties. As you step, you can see the
properties change and the plot update.

## Advancing large chunks of time

Simply call `advanceTime(duration)` where duration is the length that you want. The explorer shows
the results [^1].

## Controlling multiple clocks across processes and computers

When you have multiple processes and want to control all their clocks from a single location, you
should tell all the processes to publish their clock in some common bus. Then select the "master"
process (only one) and tell it to publish the master clock to the same bus. The master clock will
discover and control all the clocks that it finds in the bus.

For example, the processes being controlled would have something like this:

```yaml title="Controlled process configuration"
kernel:
  runMode: virtualTime
  clockBus: se.clocks
```

This makes them publish their internal clocks to the "se.clocks" bus, but they don't publish any
master clock.

In your master process you would do:

```yaml title="Master app configuration"
kernel:
  runMode: virtualTime
  clockBus: se.clocks
  clockMaster: true
```

Now you will find an object called "master" in the "se.clocks" bus that you can use to control the
global advancement of time :smiley:

[^1]: Note that the explorer is not plotting all the changes that happen to the property (you see
    some "jumps" in the plot graphic). This is expected. The reason is that the explorer is actually
    sampling the values of those properties in real time (at 60 Hz or so), while those properties
    are changing very fast.
