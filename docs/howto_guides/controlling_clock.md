# Controlling the Clock

The `Clock` is a Sen object that can published by the kernel to allow you control the virtual time.

You can enable it by adding this to your configuration file:

```yaml
kernel:
  runMode: virtualTime
  clockMaster: true
  
# ... rest of the configuration file
```

This will make the kernel publish an object called "master" in a bus (by default is "local.kernel").
You can change the bus used for publishing the clock by setting the clockBus attribute, for example:

```yaml
kernel:
  runMode: virtualTime
  clockMaster: true
  clockBus: se.clocks
  
# ... rest of the configuration file
```

The Clock class is very straightforward:

```rust
class Clock
{
  // the current virtual time
  var delta : Duration [writable];

  // calls advanceTimeDrainInputsAndUpdate(delta) and then flushOutputs()
  // on all VirtualKernelClocks n times, where n is duration / delta
  fn advanceTime(duration: Duration) -> Duration [bestEffort];

  // single step forward, based on delta
  fn step() -> Duration [bestEffort];

  // perform 'count' steps forward, based on delta
  fn steps(count: u64) -> Duration [bestEffort];
}
```

## Stepping the time

1. Start your kernel in virtualized time mode.
2. Tell it to instantiate a clock by setting `clockMaster` to `true`.
3. Find the clock and set a value for `delta`.
4. Call `step()` or `steps(n)` or `advanceTime(duration)`.

For example, here we run the "school" example and use the following kernel config:

```yaml
kernel:
  runMode: virtualTime
  clockMaster: true
```

We use the kernel clock to step the time and monitor one object (with the explorer). You can see how the 
properties change and the plot gets updated.

## Advancing large chunks of time

Simply call `advanceTime(duration)` where duration is the length that you want. The explorer shows
the results [^1].

## Controlling multiple clocks across processes and computers

When you have multiple processes and want to control all their clocks from a single location, you
should tell all the processes to publish their clock in some common bus. Then select the "master"
process (only one) and tell it to publish the master clock to the same bus. The master clock will
discover and control all the clocks that it finds in the bus.

For example, you would have something like this in your "slave" processes:

```yaml title="Slave app configuration"
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

[^1]: Note that the explorer is not plotting all the changes that happen to the property (you see some
    "jumps" in the plot graphic). This is expected. The reason is that the explorer is actually sampling
    the values of those properties in real time (at 60 Hz or so), while those properties are changing
    very fast.
