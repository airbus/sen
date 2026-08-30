![Screenshot](../assets/images/clock_light.svg#only-light){: style="width:250px; float: right;"}
![Screenshot](../assets/images/clock_dark.svg#only-dark){: style="width:250px; float: right;"}

# Execution model

In Sen your code has an entry point. This means that the rate at which it runs can be controlled,
and therefore you can switch between real-time and stepped execution (which can be slower or faster
than real-time).

Determinism[^1] is the idea that what happens next follows entirely from what came before. In this
context that becomes:

> The next state is determined completely by the computations based on the current state.

This idea of "current" and "next" state is built into the Sen architecture and great care has
been put into enabling it. In fact, all objects are double buffered and proxy objects (local and
remote) are created to allow it.

*Note:* Sen will enable deterministic behavior, but you need to do your part. For example, if you
use random numbers, do not seed your generator with the current time. Instead, make the seed part of
the input parameters used during initialization.

## The drain-update-commit cycle

To achieve determinism, Sen components execute in iterations. In each iteration they:

1. Drain their inputs to get a snapshot of the outside world. This "picture" does *not* change
   during the execution cycle. Not even time changes. The world is effectively frozen. You don't
   have to worry about this step.
2. Perform the needed *computations* and produce outputs. Outputs are write-only during this stage.
   This means that the values that you set and the events that you produce will not be visible (not
   even by you) until you commit your outputs. This is the step that you worry about.
3. Commit the outputs, so that they become visible to other components in a self-consistent manner.
   You also don't have to worry about this step.

This is the drain-update-commit approach, and you can think of this as with frames in a movie:

1. You get the current frame when Sen drains the inputs.
2. You compute the next frame based on the current one.
3. Sen prints the next frame when it commits the outputs.

Let's be a bit more precise:

______________________________________________________________________

**During the drain stage**

Sen will:

1. Update the time value (a value that you can fetch using the API).
2. Update the properties of the objects that have changed.
3. Call the methods that other components have called on your objects.
4. Perform the callbacks that have been triggered due to the emission of events.

During this stage you might update the state of an object or emit an event. That's fine. Those
changes will not be visible to anyone (including you) until your outputs are commited.

______________________________________________________________________

**During update stage**

Sen will call the `update()` function on the objects that you have created. Sen will keep track of
the properties that you have changed, the events that you have emitted and the methods that you have
called.

You can fully rely on safely fetching the state of the objects that you are tracking. Even if they
come from other components. *They will not change*. They have been created just for you. Even if the
other component is running in the same process.

______________________________________________________________________

**During commit stage**

Sen will:

1. Collect all the changes that you made to your objects and transmit them to those who are
   interested. It does it in a batch, so that when others fetch your state, they will see a fully
   consistent (atomic) change.
2. Transmit the events that you produced to those who are interested in them.
3. Request the other components to perform a method call on the objects and methods that you called
   during the update stage.

Components do not run in any order relative to one another. Each has its own thread and its own
`freqHz`, and nothing schedules one after another. The double buffer is what makes that safe: what
you read during your update was fixed before it began. A component that has to act on another one's
result therefore reads what that component last committed, never a value being computed in the same
cycle, and you express such a dependency in the data flow instead of in the schedule. `group` orders
startup and shutdown only, as [Configuration](configuration.md#componentconfig) says.

Different rates need nothing special. A slow reader sees whatever the fast component committed most
recently, and a fast reader sees the same value on every cycle until the slow one commits again.
Under `virtualTime` that does not change: `freqHz` still decides how often a component updates, and
the master clock's `delta` decides how far time moves on each step.

## Real-time execution vs stepped execution

"Real time execution" in Sen means that Sen will use the internal system clock to measure the
passage of time. When you have multiple components running at the same time, you need to decide the
mechanism you want to use to keep them in sync.

In case of real time execution, the simplest, most effective, most performant and most commonly used
approach, is to have the computers' clocks in sync using PTP (or NTP), and select an update rate
where every component updates frequently enough so that the execution progresses consistently
forward within some margin of tolerance. Be aware that this approach is *inherently
non-deterministic*, because the iterations are not fully coordinated and very much affected by the
precision of the time sync, the compute load and scheduling made by the OS, the network load and
transport delays, and a very long etc.

Stepped execution does coordinate your components. This idea translates to:

- Components do not advance with the natural passage of time. In fact, they do not advance until
  requested.
- The time that components fetch comes from an internal variable held by the kernel.

You ask for it in the configuration, with the kernel's `runMode`:

| `runMode` | Behavior |
| --------- | --------- |
| `realTime` | Components execute using system time. The default. |
| `virtualTime` | Components execute in discrete steps that you request. |
| `virtualTimeRunning` | The same, but the kernel advances the time continuously. |
| `startAndStop` | Starts everything, then stops. This is what `sen run --start-stop` sets. |

Under `virtualTime` the kernel publishes a clock object you drive, named `clock` by default and
placed on the kernel's bus unless `clockBus` says otherwise. Advancing it happens in two calls, and
this is what makes multiple processes tractable: `processNoFlush(delta)` updates the time, drains
the inputs and cycles the components *without* making their outputs visible, and `flushOutputs()`
then publishes them. Across several processes you call the first on every kernel, wait for all of
them, and only then call the second, which is how the whole system steps together rather than
drifting. Setting `clockMaster: true` publishes a master clock that discovers the individual kernel
clocks on its bus and does that coordination for you.

Within a single process, stepped execution is deterministic. Across processes it is not, yet.

Not everything can be stepped. A component that returns `true` from `isRealTimeOnly()` is left out
of the set the kernel advances: it keeps following the real clock while the rest of the system is
virtualized.

That is for components doing infrastructural work rather than simulating anything: the ones whose
job only makes sense in real time. A shell stepped along with the model would be unusable: it would
respond only when you advanced the clock, and you advance the clock from the shell. The same
reasoning covers transports, profilers and anything driven by a person or an external system.

Six of the shipped components are marked this way and keep following the real clock when the rest
of the system is stepped:

| Component | Why |
|---|---|
| [`shell`](../components/shell.md) | driven by a person, and it is where you advance the clock from |
| [`explorer`](../components/explorer.md) | driven by a person |
| [`ether`](../components/ether.md) | a transport: the network does not step |
| [`py`](../components/py.md) | runs an interpreter on its own schedule |
| [`tracy`](../components/tracy.md) | a profiler, and it measures real time |
| [`logmaster`](../components/logmaster.md) | infrastructural, and logging a stepped run is still real-time work |

Everything the kernel builds from your `build:` section is stepped. See
[Writing a component](../howto_guides/components.md#the-component-lifecycle) for how to mark one
of your own.

## The time your model sees

`getTime()` gives you the execution time, and what that time means is yours to decide: Sen moves it
along and does not interpret it. Under `realTime` it follows your component's schedule, anchored
when the component starts and advancing by exactly one period per cycle. Under `virtualTime` it
advances by that same period, aligned to multiples of it, and no cycle is ever skipped. What changes
is the stepping: you drive it, so how fast it runs against the wall clock is your choice and need
not match it at all. It is fixed for the cycle you are in, so reading it more than once during
`update()` gives the same value. If your system already keeps other clocks, you can correlate them
to this one and drive them however you like.

You drive that stepping from the master clock, where `delta` is the size of one step. `step()` takes
a single step and `steps(n)` takes several, while `advanceTime(duration)` takes as many steps of
`delta` as fit in the duration and rounds up to a whole one, so asking for 100 ms with a `delta` of
30 ms runs four steps and advances 120 ms.
[Controlling the clock](../howto_guides/controlling_clock.md) has the configuration and a worked
run.

Fixed-rate cycles are what Sen schedules today, and other scheduling modes will follow as they are
needed.

Sen is not a simulation framework. What it gives you is objects other processes can see, a cycle
that runs them and a clock you can drive, which is what a simulation framework would be built on
rather than the framework itself. Solvers, scenario handling, model libraries and scheduling
policies are yours to bring or to build. For a model that does not need any of that, what is here
may be enough on its own.

A model that advances in time differences the clock and integrates over the result. Nothing else is
needed:

```cpp
class AircraftImpl: public AircraftBase<>
{
  void update(sen::kernel::RunApi& api) override
  {
    const auto now = api.getTime();

    // There is nothing to integrate over on the first cycle, so start from the beginning.
    if (!started_)
    {
      lastUpdate_ = api.getStartTime();
      started_ = true;
    }

    const auto dt = now - lastUpdate_;
    lastUpdate_ = now;

    setNextAltitude(getAltitude() + getVerticalSpeed() * dt.toSeconds());
  }

  sen::TimeStamp lastUpdate_;
  bool started_ = false;
};
```

Subtracting two `TimeStamp` values gives a `Duration`, and `toSeconds()` turns it into a `float64_t`
you can multiply by. Because `dt` comes from the clock rather than from `freqHz`, the same code is
correct when a cycle is skipped and when the component is stepped. It is not the same run, though.
Stepping gives the same sequence of steps every time, while under `realTime` a skipped cycle merges
two steps into one, so a model with a saturation, a rate limit or a discrete event can land
somewhere else.

## When a component runs out of time

Only under real-time execution. Stepping has no deadline to miss: the kernel waits for every
component to finish its cycle, so a slow `update()` makes the run take longer without losing a
cycle. Components marked `isRealTimeOnly()` keep their deadline even in a stepped system.

A component at `freqHz: 30` gets 33 ms. If `update()` takes longer, the kernel does not stretch the
period or queue the work: it **skips whole cycles** and stays on the original schedule. A component
that overruns once loses updates but is back in step immediately, rather than accumulating delay.
The execution time follows that schedule, so it advances by a whole number of periods: two
consecutive `update()` calls can be 33 ms apart or 66, but never 41.

Cycles are skipped for one other reason. Under real-time execution the schedule is kept on the
machine's clock, so an adjustment to it, from NTP or PTP or by hand, moves the schedule too. A large
correction forward skips cycles the same way an overrun does, and a large one backward leaves the
component waiting until wall time catches up. Small corrections, which is what a synchronized
network produces, pass through without either effect.

Overruns and missed frames are reported separately, and they are not the same:

| Reported | Measured against | Where it goes |
|---|---|---|
| `<component> execution time overrun` | Thread CPU time used by the update | Tracy, and a `WARN` in the log |
| `<component> missed frame (interruption)` | Wall clock: cycles were actually skipped | Tracy only |

**The log tells you about CPU, not about lost cycles.** An update that blocks (on a socket, a lock,
a vendor SDK) burns wall time without burning CPU time, so it can miss frames without ever producing
a warning. The signal that says cycles were genuinely lost goes only to
[Tracy](../components/tracy.md), so a component running at a fraction of its configured rate looks
healthy in the log.

The warning can be suppressed from code but not from configuration: `RunApi::execLoop` takes a
`logOverruns` flag, so a component driving its own loop can drop the log line and keep the Tracy
message. Components declared under `build:` run through the kernel's standard pipeline, which does
not take the flag.

!!! note "Under development"

    Overrun handling is an area we are actively working on. What is described here is what the
    current implementation does, not a settled contract.

______________________________________________________________________

**Note**: With stepped execution the system is not only deterministic within a process, but
*multithreaded*. If components have a significant amount of work to do on each iteration, and there
are multiple cores (or computers), the usage of your compute resources improves. This is a nice (and
non-accidental) side effect of the drain-update-commit approach used with threads and processes.

**Also Note**: Having multiple processes implies a higher synchronization overhead. Data flows have
to be synchronized not only between threads, but between computers. If the work that components do
is *outweighed* by the synchronization overhead, the result is a *pessimization*. Pessimizations
are common when trying to parallelize, as many factors come into play. Luckily, Sen allows
you to compose your system as you need without touching a single line of code, so you will have an
easy time shaping your configuration according to the performance profile of your computations.

[^1]: Not a formal definition, just a way to convey the idea. PhDs have been written about this.
