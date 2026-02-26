![Screenshot](../assets/images/clock_light.svg#only-light){: style="width:250px; float: right;"}
![Screenshot](../assets/images/clock_dark.svg#only-dark){: style="width:250px; float: right;"}

# Execution Model

In Sen your code has an entry point. This means that the rate at which it runs can be controlled,
and therefore we can switch between real-time and stepped execution (which can be slower or faster
than real-time).

Determinism[^1] is the idea that:

> "Everything that happens in the world is determined completely by previously existing causes."

In our context, this translates to the idea that:

> "The next state is determined completely by the computations based on our current state."

This idea of "current" and "next" state is built-in into the Sen architecture and great care has
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

## Real-Time execution vs Stepped execution

"Real time execution" in Sen means that Sel will use the internal system clock to measure the
passage of time. When you have multiple components running at the same time, you need to decide the
mechanism you want to use to keep them in sync.

In case of real time execution, the simplest, most effective, most performant and most commonly used
approach, is to have the computers' clocks in sync using PTP (or NTP), and select an update rate
where every component updates frequently enough so that the execution progresses consistently
forward within some margin of tolerance. Be aware that this approach is *inherently
non-deterministic*, because the iterations are not fully coordinated and very much affected by the
precision of the time sync, the compute load and scheduling made by the OS, the network load and
transport delays, and a very long etc.

In stepped execution, we do coordinate the execution of our components. This idea translates to:

- Components do not advance with the natural passage of time. In fact, they do not advance until
  requested.
- The time that components fetch comes from an internal variable held by the kernel.

When we are controlling multiple components that execute in multiple processes, the approach is very
similar. We first do step 1 for all processes, and when finished, we do step 2 for all processes.

In both cases (multiple or just one process), there is an API that we can use to tell Sen to
sequentially execute multiple steps and therefore advance large chunks of time in a synchronized
manner.

______________________________________________________________________

**Note**: You can see that with stepped execution we are not only deterministic, but
*multithreaded*. This means that if components have some significant amount of work to do on each
iteration, and we have multiple cores (or computers), we are effectively improving the usage of our
compute resources. This is a nice (and non-accidental) side effect of the drain-update-commit
approach used with threads and processes.

**Also Note**: Having multiple processes implies a higher synchronization overhead. We not only have
to synchronize data flows between threads, but computers. If the work that components do is
*outweighed* by the synchronization overhead, then we are doing a *pessimization*. Pessimizations
are common when trying to parallelize, as many factors come into play. Luckily for us, Sen allows
you to compose your system as you need without touching a single line of code, so you will have an
easy time shaping your configuration according to the performance profile of your computations.

[^1]: Not a formal definition; just a way to convey the idea. Phds have been written about this.
