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

Apart from this, you can also implement a function that will get called when the object gets
unregistered:

```c++ title="MyClass de-registration"
void unregistered(sen::kernel::RegistrationApi& api) override
{
  // do something once (and maybe use the api)
}
```

### Updates

If your object is published to a bus, an `update()` function will be called every time, after the
`drainInputs()` gets called. This allows you to perform periodic updates to your internal state and
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

:man_raising_hand: The `sen::kernel::RunApi`, like the name indicates, is the runtime API that
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

Sen is fully asynchronous, this means that calls to methods do not block. This is easy when your
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
  catch(const std::runtime_error& err)
  {
    // do something with err
    std::cout << err.what() << std::endl;
  }
}
```

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
auto cb = [&]() { std::cout << "position changed: " << myObject->getPosition() << "\n"; };
myObject->onPositionChanged({this, std::move(cb)}).keep();
```

These methods register a callback and return a `ConnectionGuard` object. The returned
`ConnectionGuard` object represents this registration.

NOTE: The call to `.keep()` keeps the connection established, even after the `ConnectionGuard`
object was destroyed.

If you don't have the generated code, and see yourself working with generic proxies, you can use the
`onPropertyChangedUntyped` method, which is also available in all objects, but works with Variants.

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

If you need to obtain data coming from other sources (the environment). Those sources can be (1) the
kernel or (2) other components.

The kernel can give you the following information:

- Your application name via `KernelApi::getAppName()`.
- The known types via `KernelApi::getTypes()` (You won't normally need to use this, as it is aimed
  towards tooling).
- The configuration passed by the user via `ConfigGetter::getConfig()`.
- Whether you are required to stop, via `RunApi::stopRequested()`.
- The current (virtualized) time via `RunApi::getTime()`.
- The time the component's objects started from, via `RunApi::getStartTime()`.
- The configured cycle time, when one is set, via `RunApi::getTargetCycleTime()`.

[The execution model](../users_guide/execution_model.md#the-time-a-component-sees) explains what that
time is, how it moves in each run mode and how a model uses it.

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

Finally, you need some storage where to put those discovered objects. The `sen::ObjectList` class
serves this purpose.

The code to discover objects on a bus could look like this.

Subscribe to the generated `<Class>Interface` type, never to an implementation class. Sen generates
an interface for every class you declare in STL, and that is what other objects hold: a remote
object has no implementation on your side to name. [The generated code](generated_code.md) covers
the pair.

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

You can also use the Sen Query Language to get certain objects. For example:

```cpp title="Using SQL to discover objects"
// declare your interest
auto interest = sen::Interest::make("SELECT my_package.MyClass from local.test", api.getTypes());

// get the source
auto bus = api.getSource(interest->getBusCondition().value());

// create a container
sen::ObjectList<sen::Object> objects;

// subscribe our container
bus->addSubscriber(interest, &objects, true);
```
