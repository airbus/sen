# Unit testing

Sen ships a `TestKernel` and a `TestComponent` so your packages can be driven from an ordinary unit
testing framework, without a running system. Both are declared in
`libs/kernel/include/sen/kernel/test_kernel.h`.

`TestKernel` starts in virtual time, so a test decides when time advances instead of waiting for it.
You move it forward with `step()`, one cycle at a time.

## The modes

A `TestKernel` runs in one of two modes, chosen when you construct it and never from configuration.

Virtual time is the default and what the rest of this page uses. Components advance only when you
call `step()`, so the test decides when time passes and the same test does the same thing every run.
Real time lets the components free-run on their own threads, which is how you exercise several
components at different rates, or anything whose behaviour depends on real interleavings.

The two are not interchangeable. `step()`, `stepUntil()` and `getTime()` abort on a real-time
`TestKernel`, because there is no virtual clock to advance or to read. `stopRequested()` differs
too. Stepping ignores a stop request, so a virtual-time test can use it as a completion signal,
while a real-time `TestKernel` honors it and tears itself down.

## The double step

If your code reacts to property-changed callbacks, you need **two** steps before the callback runs -
`kernel.step()` twice, or `kernel.step(2)`.

To see why, look at how a cycle works.

![Screenshot](../assets/images/two_ticks_light.svg#only-light)
![Screenshot](../assets/images/two_ticks_dark.svg#only-dark)

The properties changed during a cycle are collected in the commit phase ❶, and become visible in
the drain phase ❷ of the *next* cycle.

When a test pushes an update, it does so between kernel steps ❸, while the kernel is waiting. The
queue of changed properties for that cycle has already been built, so the change needs one more step
to reach the object's getter, and another for the callback that reports it.

## Example

The code below is the real test Sen runs against its own kernel, included from
`libs/kernel/test/unit/test_kernel/test_kernel_test.cpp`. It compiles and passes in CI, so it cannot
drift from the API it demonstrates.

First the setup: an object, a component that publishes it, and callbacks that count what arrives.

```c++
--8<-- "libs/kernel/test/unit/test_kernel/test_kernel_test.cpp:setup"
```

`TestComponent` lets you supply the component's phases as lambdas instead of writing a subclass.
`onInit` gets an `InitApi` and is where objects are published to a bus; `onRun` gets a `RunApi` and
usually ends in `execLoop`, which drives a cycle at the given period. The callback passed to
`execLoop` is optional, and runs once per cycle.

Note `.keep()` on the callback registrations. They return a `ConnectionGuard`, and dropping it
unregisters the callback immediately.

Now the double step, with the assertions that show it:

```c++
--8<-- "libs/kernel/test/unit/test_kernel/test_kernel_test.cpp:delay"
```

Read the two blocks together. After the first `step()` the property has already changed -
<code>get&lt;<var>Prop</var>&gt;()</code> returns 1, but `propCount` is still 0, because the
callback has not run yet. It takes the second `step()` for the change to be reported. That is the
double step, and it is why a test that checks a callback after a single step will see nothing.

## Waiting without sleeping

`stepUntil(predicate, maxSteps)` steps until your predicate holds, and returns the number of steps
it took. If the budget runs out while the predicate is still false it throws, so a test that never
reaches its condition fails with a message instead of looping until someone kills it.

`waitForQuiet()` blocks until the kernel server and the inbound session lanes have nothing queued or
running. Read its limits before relying on it: it covers the control-plane queues only, it does not
consult component mailboxes, traffic still inside a transport can arrive immediately afterwards, and
it does not advance time. `step()` drives time; this only waits out kernel-side work.

## Reading state from a test

Reaching an object through `getComponentBus()` and calling its getter is safe in one situation:
virtual-time mode, at a step boundary, on state owned by a component the virtual clock drives. There
the writer is idle and the step you just returned from is what makes its last write visible to you.

Reading `local.kernel` this way is never safe. The kernel component runs in real time whichever mode
you chose, so it may be writing while you read. Use `fetchStats()` instead. It returns a snapshot of
the kernel's counters and is safe to call from the test thread at any moment.

Everything else needs a fixture component: real-time mode, objects owned by another process,
observing mid-step, or an assertion that has to span several objects at one instant. The fixture
subscribes to what you care about and captures the values on its own runner thread, and your test
reads the fixture's copies instead of the live objects.
