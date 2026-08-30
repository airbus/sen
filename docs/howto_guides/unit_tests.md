# Unit testing

Sen ships a `TestKernel` and a `TestComponent` so your packages can be driven from an ordinary unit
testing framework, without a running system. Both are declared in
`libs/kernel/include/sen/kernel/test_kernel.h`.

`TestKernel` starts in virtual time, so a test decides when time advances rather than waiting for
it. You move it forward with `step()`, one cycle at a time.

__HINT__: This type of testing suits simple tests. It is not the tool for several components running
at different rates, or for deferred methods.

## The double step

If your code reacts to property-changed callbacks, you need **two** steps before the callback runs -
`kernel.step()` twice, or `kernel.step(2)`.

To see why, look at how a cycle works.

![Screenshot](../assets/images/two_ticks_light.svg#only-light)
![Screenshot](../assets/images/two_ticks_dark.svg#only-dark)

The properties changed during a cycle are collected in the commit phase ❶, and become visible in the
drain phase ❷ of the *next* cycle.

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

Note `.keep()` on the callback registrations: they return a `ConnectionGuard`, and dropping it
unregisters the callback immediately.

Now the double step, with the assertions that show it:

```c++
--8<-- "libs/kernel/test/unit/test_kernel/test_kernel_test.cpp:delay"
```

Read the two blocks together. After the first `step()` the property has already changed -
<code>get&lt;<var>Prop</var>&gt;()</code> returns 1, but `propCount` is still 0, because the
callback has not run yet. It takes the second `step()` for the change to be reported. That is the
double step, and it is why a test that checks a callback after a single step will see nothing.
