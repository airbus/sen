# Working with Objects

## Implementing your objects

### Registration

If you need to do some work after construction, but just before your object starts to get called,
you can overwrite the `registered()` function. For example: 4

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
void MyClass::update(sen::kernel::RunApi& runApi)
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

If you need finer control over what happens during the drain-compute-flush cycle, you can implement
the following functions. For example, this would help if you need to perform some action before the
draining inputs the component where the object lives. Note that the needsPreDrainOrPreCommit()
function should return true for these functions to be called.

```c++
void preDrain() override
{
  std::cout << "after this, Sen will drain the inputs\n";
}

void preCommit() override
{
  std::cout << "after this, Sen will flush the outputs\n";
}

bool needsPreDrainOrPreCommit() const noexcept override
{
  return true;  // 'true' means preDrain() and preCommit() will be called
}
```

## Interacting with objects

### Calling Methods

Sen is fully asynchronous, this means that calls to methods do not block. This is easy when your
method does not return any value, but if it does (and you are interested in it), then you need to
provide a callback. For example:

```c++ title="Calling a method"
  myObject->divideNumbers(2, 2, {this, [&](const auto& response)
  {
    if (response)
    {
      std::cout << response.getValue() << "\n";
    }
    else
    {
      std::cout << "error\n";
    }
  }});
```

#### Handling responses and errors

Methods may or may not return values. If you want to obtain the returned value, you can use the
`response` argument. It's a `MethodResult<T>` from which you can extract the value (using
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

### Deferred Methods

In Sen, you can postpone the execution of a method by declaring it `deferred`. This will sightly
change the signature of the generated code in the *implementation* side (users keep the same API).

For example, imagine the following STL:

```rust
class Calculator
{
  // If b != 0 then returs a / b. Throws otherwise.
  fn divideNumbers(a: i32, b: i32) -> i32 [deferred];
}
```

The generated code will look like this:

```c++
virtual void divideNumbers(int32_t a, int32_t b, std::promise<int32_t> result) = 0;
```

So, instead of having to return, you are now able to store the promise and defer the computation to
eventually call `result.set_value()` with the result.

For example:

```c++ title="Postponing work"
void divideNumbers(int32_t a, int32_t b, std::promise<int32_t> result)
{
  // defer the work by storing it in a queue of std::function<void()>
  pendingWork_.push_back([a, b, result = std::move(result)]() mutable {
    if (b != 0)
    {
      result.set_value(a/b)
    }
    else
    {
      auto err = "divideNumbers would cause a division by 0";
      result.set_exception(std::make_exception_ptr(std::runtime_error(err)));
    }
  });
}
```

Apart from storing the work for deferred execution, you can also use this mechanism to forward the
call to another object. For example:

```c++ title="Forwarding a call"
void divideNumbers(int32_t a, int32_t b, std::promise<int32_t> result)
{
  // we can hand over the work to another object
  otherCalculator_->divideNumbers(a, b, {this, [ourResult = std::move(result)](const auto& theirResponse)
  {
    if (theirResponse.isOk())
    {
      ourResult.set_value(theirResponse.getValue());
    }
    else
    {
      ourResult.set_exception(theirResponse.getError());
    }
  }});
}
```

### Reacting to Property changes

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

### Reacting to Events

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

The kernel can provide us the following information:

- Our application name via `KernelApi::getAppName()`.
- The known types via `KernelApi::getTypes()` (You won't normally need to use this, as it is aimed
  towards tooling).
- The configuration passed by the user via `ConfigGetter::getConfig()`.
- If we are required to stop via `RunApi::stopRequested()`.
- The current (virtualized) time via `RunApi::getTime()`. More on this later.

We can also actively ask the kernel:

- To stop, via `KernelApi::requestKernelStop(int exitCode)`.
- To get us to process incoming information, via `RunApi::drainInputs()`.
- To flush outgoing information, via `RunApi::commit()`.
- To provide us pointers to objects that were published by other components and to let us publish
  our own objects.

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

The code to see the objects that are published by the kernel in the "local.tutorial" bus could look
like this.

```cpp title="Using object sources to discover objects"
sub_ = api.selectAllFrom<MySubClassImpl>("local.tutorial");

// you can now use sub_.list to iterate over the objects,
// (which will be automatically populated), and also add
// callbacks like:
//   sub_->list.onAdded(...);
//   sub_->list.onRemoved(...);
// For example:

std::ignore = sub_->list.onAdded(
  [this](const auto &iterators) {
    for (auto it = iterators.typedBegin; it != iterators.typedEnd; ++it)
    {
      (*it)->onSomethingHappened({this, [](){ std::cout << "something happened!\n"; }}).keep();
    }
  });
```

NOTE: Keep in mind the subscription and the registered callbacks are tied to the lifetime of the
subscription object, so when you want to keep a subscription alive you have to store it for the
required time. For example, when the subscription should be alive as long as your component is
running, you can store it as a member of your component and initialize it in the
`my_component::register(...)` function of your component.

This `sub_.list` container is a `sen::ObjectList<T>` which acts like an enhanced `std::vector<T>`
where `T` can be the specific type of the objects that you are interested in.

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
