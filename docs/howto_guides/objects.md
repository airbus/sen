# Working with objects

The examples on this page come from the shipped example packages, `calculators`, `lifecycle` and
`fibonacci`. Where a snippet is written out rather than transcluded, it uses `my_package.MyClass`
from [Create your first package](../getting_started/first_package.md).

## Implementing your objects

### Initial values

A property staged in your constructor with `setNext...()` is published by the first commit, which
happens before the first cycle begins. That makes the constructor a place to set initial values in
code, as an alternative to setting them in the YAML configuration.

### Registration

If you need to do some work after construction, but just before your object starts to get called,
you can overwrite the `registered()` function. For example:

```c++ title="MyClass registration"
void registered(sen::kernel::RegistrationApi& api) override
{
  // do something once (and maybe use the api)
}
```

The registration API lets you obtain sources and register for discovering other objects that you
might be interested in.

`registered()` runs when your object is added to a bus. Everything the kernel builds from a
configuration file is added, so for those objects it always runs, and whatever you acquire there is
in place for every later call, including the destructor. An object you create in code and never add
to a bus is never registered, and anything `registered()` would have given it stays empty.

Apart from this, you can also implement a function that will get called when the object gets
unregistered:

```c++ title="MyClass de-registration"
void unregistered(sen::kernel::RegistrationApi& api) override
{
  // do something once (and maybe use the api)
}
```

### Updates

If your object is published to a bus, your `update()` is called once per cycle, after
`drainInputs()`. This allows you to perform periodic updates to your internal state and
trigger your internal logic. For example:

```c++ title="my_class.cpp"
void MyClassImpl::update(sen::kernel::RunApi& runApi)
{
  // change prop2 with some dummy values
  StructOfInts val = getProp2();
  val.field1 += 1;
  val.field2 += 2;
  setNextProp2(val);
}
```

The `sen::kernel::RunApi`, as the name suggests, is the runtime API that
allows your component to interact with Sen. It only has a few (but powerful) methods.

### Advanced callbacks

If you need finer control over what happens during the drain-update-commit cycle, you can run code
just before the drain and just before the commit.

**`needsPreDrainOrPreCommit()` returns `false` by default**, and the two hooks are only called when
it returns true. Overriding the hooks alone does nothing at all, silently, which is the easiest
mistake to make here.

The code below is from the `lifecycle` example package, which builds as part of the examples. Its
interface is small: it counts the phases and publishes what it saw:

```rust
--8<-- "examples/packages/lifecycle/stl/lifecycle/phase_recorder.stl"
```

```c++
--8<-- "examples/packages/lifecycle/src/phase_recorder.h:hooks"
```

`preCommit()` is the more useful of the two, because it is the last point at which a property staged
with `setNext...()` still reaches the commit at the end of that same cycle. That lets an object
publish something derived from everything that happened during the cycle:

```c++
--8<-- "examples/packages/lifecycle/src/phase_recorder.cpp:precommit"
```

Running that example shows the ordering. **The first commit happens before any drain or update**,
because the kernel commits the initial state of your objects before the first cycle begins. So
`preCommit()` is called once more than the other two.

```text
cycles=1  lastCycle="preDrain=0 update=0 preCommit=1"
cycles=2  lastCycle="preDrain=1 update=1 preCommit=2"
```

### Object naming convention

Sen supports all special characters in published object names, with the single exception of literal
space characters (`" "`), which are restricted.

## Interacting with objects

### Calling methods

Sen is fully asynchronous, which means that calls to methods do not block. This is easy when your
method does not return any value, but if it does (and you are interested in it), then you need to
provide a callback. For example:

```c++ title="calculators/src/client.cpp"
--8<-- "examples/packages/calculators/src/client.cpp:async_call"
```

That is the whole of a real client: find a calculator, call `add` on it, and handle the answer when
it arrives. The `{this, handleResult}` pair is the callback. `this` is what ties the callback to
your object's lifetime, so it stops being invoked if your object goes away.

#### Handling responses and errors

Methods may or may not return values. If you want to obtain the returned value, you can use the
`result` argument. It's a `MethodResult<T>` from which you can extract the value (using
`getValue()`) or the error (using `getError()`).

Note that if methods do not return any value, you can still register a callback and react to the
fact that the method execution is finished (or detect any errors).

```c++
if (result.isOk())
{
  // it worked, do something with the return value
  std::cout << result.getValue() << std::endl;
}
else
{
  // Something went wrong - try to find out what.
  // The error is an `std::exception_ptr` that we can re-throw
  // when interested in knowing more about it.
  try
  {
    std::rethrow_exception(result.getError());
  }
  catch(const std::exception& err)
  {
    // do something with err
    std::cout << err.what() << std::endl;
  }
}
```

Choose what you catch. `std::exception` covers everything a call can deliver; a narrower type covers
only what you name, and what it does not match escapes the callback. Within one process the error
arrives as the type the method threw. Across a process boundary that type cannot travel, so what
reaches you is a `std::logic_error`, a `std::invalid_argument`, a `std::runtime_error` or a bare
`std::exception`. [Threading and object
lifetime](../users_guide/threading.md#what-survives-a-process-boundary) has the mapping.

If you don't install any callback, you will be effectively ignoring the result of the method call.
This includes any potential errors signaled by the method.

### Deferred methods

A method normally returns its value, and Sen delivers it to the caller. Sometimes you cannot answer
straight away: the work is slow, or it has to be handed to someone else. Marking a method *deferred*
changes the generated implementation side: you receive a `std::promise` and fulfill it whenever you
are ready. Callers see no difference.

A method can be marked deferred in two ways. The direct one is the `[deferred]` attribute in the
STL:

```rust
fn computeFibonacci(n: u32) -> u64 [deferred];
```

The other is the code generation settings, which keeps the STL free of implementation choices, so
the same interface can be implemented with or without deferral. That is what the `fibonacci` example
does, so it is the one shown here. Its STL declares an ordinary method:

```rust
--8<-- "examples/packages/fibonacci/stl/fibonacci.stl"
```

and the settings file names the ones to defer:

```json
--8<-- "examples/packages/fibonacci/src/codegen_settings.json"
```

which the package passes with `CODEGEN_SETTINGS`. See
[CMake integration](../users_guide/cmake.md#to-build-a-package). See also
[Customizing the generated code](../users_guide/stl.md#customizing-the-generated-code), which covers
the other settings this file can carry.

The generated declaration is then `computeFibonacciImpl`, taking the promise by rvalue reference:

```c++
virtual void computeFibonacciImpl(u32 n, std::promise<u64>&& promise) = 0;
```

Note the name. Callers still use `computeFibonacci()`; the generated base turns that into a
`computeFibonacciImpl()` on your class, and that is the one you implement.

The `fibonacci` example ships two implementations. The first does the work and fulfills the promise
when it is done:

```c++ title="doing the work"
--8<-- "examples/packages/fibonacci/src/fibonacci.cpp:worker"
```

The second hands the call to another object and passes the promise into the callback, so whoever
answers fulfills the original caller's promise:

```c++ title="forwarding the call"
--8<-- "examples/packages/fibonacci/src/fibonacci.cpp:forward"
```

The lambda must be `mutable`: it owns the promise, and `set_value()` is not `const`. Note also that
a promise cannot be copied, so it cannot be stored in a `std::function`, which requires a
copy-constructible target. Sen's own callback type accepts move-only targets, which is why passing
the promise into the callback above works.

### Reacting to property changes

All Sen objects allow hooks that you can use to react to property changes. The generated function
will be named as `onXChanged(callback)`, where `X` would be the name of the property.

For example:

```c++ title="Reacting to property changes"
auto cb = [myObject]() { std::cout << "position changed: " << myObject->getPosition() << "\n"; };
myObject->onPositionChanged({this, std::move(cb)}).keep();
```

The lambda captures `myObject` by value. A kept connection outlives the scope that created it, so a
reference capture would dangle. The `this` in `{this, std::move(cb)}` does a different job. It names
the object that owns the registration, and the lambda never reads it.

These methods register the callback and return a `ConnectionGuard` that owns the registration.

NOTE: The call to `.keep()` keeps the connection established, even after the `ConnectionGuard`
object was destroyed. What ends it then is the object named in the token, the `this` above: while
that object is alive the callback keeps being invoked, and once it is destroyed the callback stops.
[Threading and object lifetime](../users_guide/threading.md) covers what that guarantees.

If you do not have the generated code, and find yourself working with generic proxies, you can use
the `onPropertyChangedUntyped` method, which is also available in all objects, but works with
Variants.

### Reacting to events

In the same way as with properties, you can register as interested in an event by using the
corresponding generated function.

```c++ title="Listening to an event"
auto cb = [](int32_t arg) { std::cout << "something happened " << arg << "\n"; };
myObject->onSomethingHappened({this, std::move(cb)}).keep();
```

In this case, the callback is expected to receive the same arguments as the event generates.

As with properties, there's a Variant-based option in case you don't have the generated code:
`onEventUntyped()`.

## Runtime API

Some of what you need comes from outside your own objects: from the kernel, and from other
components.

The kernel can give you the following information:

- Your application name via `KernelApi::getAppName()`.
- The known types via `KernelApi::getTypes()` (You won't normally need to use this, as it is aimed
  at tooling).
- The configuration passed by the user via `ConfigGetter::getConfig()`.
- Whether you are required to stop, via `RunApi::stopRequested()`.
- The current (virtualized) time via `RunApi::getTime()`.
- The time the component's objects started from, via `RunApi::getStartTime()`.
- The configured cycle time via `RunApi::getTargetCycleTime()`, which is set once `execLoop` is
  running and empty for a component that drives its own loop.

[The execution model](../users_guide/execution_model.md#the-time-a-component-sees) explains what
that time is, how it moves in each run mode and how a model uses it.

You can also ask the kernel:

- To stop, via `KernelApi::requestKernelStop(int exitCode)`.
- To process incoming information, via `RunApi::drainInputs()`.
- To flush outgoing information, via `RunApi::commit()`.
- To give you pointers to objects published by other components, and to let you publish your own
  objects.

The last point is the most interesting for this section because it is where the largest part of the
communication happens.

In the Sen API, *Object Sources* are entities that you can use to publish your own objects and be
informed about other objects. This is implemented in the `sen::ObjectSource` class.

You can see that it has the methods `add(Object)` and `remove(Object)`, and it inherits from
`sen::ObjectFilter`, which contains the `addSubscriber` and `removeSubscriber` methods, that you can
use to discover objects based on some criteria that you can define.

*Object Sources* can be created by calling the method `KernelApi::getSource()`. This function takes
a string as an argument, and it expects it to be in the format `<session>.<bus>`. For example, the
kernel always publishes some objects in the "local.kernel" bus.

Finally, you need somewhere to put those discovered objects. The `sen::ObjectList` class
serves this purpose.

Subscribe to the generated `<Class>Interface` type, never to an implementation class. Sen generates
an interface for every class you declare in STL, and that is what other objects hold: a remote
object has no implementation on your side to name. [The generated code](generated_code.md) covers
the pair.

Discovering objects on a bus then looks like this:

```c++ title="calculators/src/client.cpp"
--8<-- "examples/packages/calculators/src/client.cpp:subscribe"
```

The member it assigns to is declared as
`std::shared_ptr<sen::Subscription<CalculatorInterface>> calculators_;`, and reading it is
`calculators_->list.getObjects()`. [Tutorial 2](../tutorials/two_objects.md) shows the same file
in full, with both lines annotated.

`selectAllFrom` also takes an optional callback, invoked during drain with the objects that have
just appeared, which is useful when you want to react to a discovery instead of polling the list.

NOTE: Keep in mind the subscription and the registered callbacks are tied to the lifetime of the
subscription object, so when you want to keep a subscription alive you have to store it for the
required time. For example, when the subscription should be alive as long as your component is
running, you can store it as a member of your component and initialize it in the
`registered()` function of your object.

This `calculators_->list` container is a `sen::ObjectList<T>` which acts like an enhanced
`std::vector<T>` where `T` can be the specific type of the objects that you are interested in.

The same discovery is available one level down, where you build the interest yourself and own the
container. `selectAllFrom` is this with the lifetime taken care of for you, so reach for the lower
level when you need to build the query at run time, or when you want to own the list.

```cpp title="test/util/query_test/src/component.cpp"
--8<-- "test/util/query_test/src/component.cpp:subscribe"
```

The list it subscribes is declared as `sen::ObjectList<query_test::QueryTestClassInterface>
objectsInError_;`, a member. `addSubscriber` keeps the address of your container rather than a copy
of it, so the container has to outlive the subscription. That is the rule stated above, with nothing
holding the list on your behalf. The source can stay a local, as it is here: the kernel owns the
bus, and you only need to keep the handle if you use it again later, as the school example does to
remove its objects.

`onAdded` on the list is the same idea as the callback `selectAllFrom` takes, and the query shows a
`WHERE` clause narrowing the interest to objects whose property has a particular value.
