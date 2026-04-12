# Tutorial 2: Two Objects Talking

In the previous tutorial you built a single object and inspected it in the shell. Now you will build
two objects that find each other at runtime and communicate — one calls a method on the other, and
handles the result asynchronously.

**What you'll learn:**

- How objects discover each other using subscriptions and interests
- How to call a method on a remote object and handle the result
- Why method calls in Sen are always asynchronous — and how to work with that
- The `Subscription<T>` pattern and why it must be kept alive

**Prerequisites:** Completed Tutorial 1, comfortable with STL → C++ → YAML flow.

---

## What we're building

A `Calculator` class with add and divide methods, and a `Client` class that finds a calculator on
the bus and calls its methods. Both run in the same component, on the same bus.

---

## The core pattern: discovery and subscription

In Sen, objects do not connect to each other by address. There is no "connect to `host:port`" or
"subscribe to a topic name". Instead:

1. Objects **publish themselves** to a bus when they are registered.
2. Other objects declare an **interest** — a filter that describes what they are looking for (by
   class, name, or both).
3. Sen gives them a **`Subscription<T>`** — a live list that is automatically populated with
   matching objects as they appear and cleaned up as they disappear.

This is reactive. You don't poll for objects. The list updates itself during the drain stage of each
cycle. Your code just reads from the list whenever it needs to.

!!! note "Subscriptions are live"
    If a new `Calculator` appears on the bus mid-execution (because a new component started), it
    will appear in your subscription list on the next drain. If it disappears (component stopped),
    it will be removed. You never hold a dangling pointer to a dead object.

---

## Step 1: Define the interface

Create a file `stl/calculators/calculator.stl`:

```{ .rust .annotate }
package calculators;

class Calculator
{
    var current : f64;               // (1)!

    fn add(a: f64, b: f64) -> f64;
    fn divide(a: f64, b: f64) -> f64;
    fn addWithCurrent(value: f64) -> f64;
}

class Client
{
  // Where to find the calculator objects.
  var calcBus : string [static];  // (2)!

  // Uses any available calculator.
  fn useCalculator();
}
```

1. Stores the running total. Read-only to other objects — only `CalculatorImpl` can change it.
2. The bus name where this client should look for calculators. Configured in YAML.

---

## Step 2: Implement the Calculator

```{ .cpp .annotate }
// calculator_impl.h
#pragma once

#include "stl/calculators/calculator.stl.h"

namespace calculators {

class CalculatorImpl : public CalculatorBase
{
public:
    SEN_NOCOPY_NOMOVE(CalculatorImpl)
    using CalculatorBase::CalculatorBase;
    ~CalculatorImpl() override = default;

protected:
    float64_t addImpl(float64_t a, float64_t b) override;
    float64_t divideImpl(float64_t a, float64_t b) override;
    float64_t addWithCurrentImpl(float64_t value) override;
};

} // namespace calculators
```

```{ .cpp .annotate }
// calculator_impl.cpp
#include "calculator_impl.h"

namespace calculators {

float64_t CalculatorImpl::addImpl(float64_t a, float64_t b)
{
    const float64_t result = a + b;
    setNextCurrent(result);  // (1)!
    return result;
}

float64_t CalculatorImpl::divideImpl(float64_t a, float64_t b)
{
    if (b == 0.0)
    {
        throw std::runtime_error("division by zero");  // (2)!
    }

    const float64_t result = a / b;
    setNextCurrent(result);
    return result;
}

float64_t CalculatorImpl::addWithCurrentImpl(float64_t value)
{
    const float64_t result = getCurrent() + value;
    setNextCurrent(result);
    return result;
}

SEN_EXPORT_CLASS(CalculatorImpl)

} // namespace calculators
```

1. Stage the new value. It won't be visible until commit, but the return value is delivered to the
   caller immediately when their next drain processes the method result.
2. Throwing from a method implementation is fine. Sen catches it, wraps it in a `MethodResult`, and
   delivers it to the caller's callback — the caller can inspect and re-throw if needed.

---

## Step 3: Implement the Client

This is where the interesting part happens. The `Client` needs to:

1. Find `Calculator` objects on a bus
2. Call methods on them and handle results

```{ .cpp .annotate }
// client.cpp
// generated code
#include "stl/calculator.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

namespace calculators {

class ClientImpl : public ClientBase
{
public:
  SEN_NOCOPY_NOMOVE(ClientImpl)
  using ClientBase::ClientBase;
  ~ClientImpl() override = default;

public:
  void registered(sen::kernel::RegistrationApi& api) override  // (1)!
  {
    calculators_ = api.selectAllFrom<CalculatorInterface>(getCalcBus()); // (3)!
  }

protected:
  void useCalculatorImpl() override
  {
    if (const auto& list = calculators_->list.getObjects(); !list.empty())  // (4)!
    {
      auto handleResult = [](sen::MethodResult<float64_t> result) {  // (6)!
        if (result.isOk())
        {
          std::cout << "add(3, 4) = " << std::to_string(result.getValue()) << std::endl;
        }
        else
        {
          std::cout << "add failed" << std::endl;
        }
      };

      // Call add(3, 4) and handle the result  (5)!
      list.front()->add(3.0, 4.0, {this, handleResult});
    }
  }

private:
  std::shared_ptr<sen::Subscription<CalculatorInterface>> calculators_;  // (2)!
};

SEN_EXPORT_CLASS(ClientImpl)

}  // namespace calculators
```

1. `registered()` is called once after the object is added to the bus. This is the right place to
   set up subscriptions — the object is live and the API is available.
2. **This must be a member variable**, not a local. `Subscription<T>` must stay alive as long as
   you want to receive updates. If it goes out of scope, the list is destroyed and callbacks stop.
3. `api.selectAllFrom<CalculatorInterface>(getCalcBus())` matches *all* objects that implement `CalculatorInterface`.
4. Always guard against an empty list. Objects can disappear between cycles.
5. `list.front()` returns a reference to the first matching object. The reference is valid for this
   entire update cycle (it was frozen during drain).
6. The callback lambda receives a `MethodResult<float64_t>` — either a value or an exception. It fires
   during the drain stage of a future cycle, once the result has been committed by the calculator.

---

## Step 4: The async call timeline

This is the most important thing to internalize:

```
Cycle N — Client update:
    calc.add(3.0, 4.0, callback)
    ↑ call is QUEUED, not executed yet. Returns immediately.

Cycle N — Client commit:
    the queued call is transmitted to Calculator

Cycle N+1 — Calculator drain:
    addImpl(3.0, 4.0) executes
    result is staged

Cycle N+1 — Calculator commit:
    result is transmitted back to Client

Cycle N+2 — Client drain:
    callback fires with result = 7.0
```

!!! warning "Don't expect immediate results"
    If you call a method and check for results on the very same cycle, you will find nothing. The
    result arrives 1–2 cycles later depending on scheduling. This is by design — it is what makes
    Sen thread-safe without locks.

!!! tip "Callbacks can capture `this`"
    Your callback lambda can capture `this` to store the result or trigger further actions:
    ```cpp
    calc.add(a, b, {this, [this](auto r)
    {
        if (r.isOk())
        {
            lastResult_ = r.getValue();
        }
    }});
    ```
    Sen ensures the callback is only invoked while your object is still alive — if your object
    is unregistered before the result arrives, the callback is silently dropped.

---

## Step 5: Configure and run

```{ .yaml .annotate }
load:
  - name: shell
    group: 2
    open: [local.example]

build:
  - name: myComponent
    freqHz: 5
    imports: [calculators]
    group: 3
    objects:
      - class: calculators.CasioCalculator
        name: calc1
        bus: my.tutorial
        model: superCalc

      - class: calculators.ClientImpl
        name: client1
        calcBus:  my.tutorial  # (1)!
        bus:  my.tutorial
```

1. Tells the client which bus to search for calculators. Must match the bus where `calc1` is
   published.

```sh
cmake -S . -B build && cmake --build build
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/build/bin
sen run config.yaml
```

In the shell you should see `add(3, 4) = 7` printed when calling `my.tutorial.client1.useCalculator`. You
can also directly use the calculator:

```sh
info my.tutorial.calc1
my.tutorial.calc1.add 10, 5
my.tutorial.calc1.getCurrent
```

---

## Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| `Subscription` as a local variable | List is always empty | Make it a member variable |
| Reading result on the same cycle as the call | Result is never seen | Handle it in the callback |
| Objects on different buses | Client list is always empty | Ensure `calcBus` matches the bus in YAML |
| Missing `SEN_EXPORT_CLASS` | Class not found by kernel | Add the macro at the bottom of the `.cpp` |

---

## What just happened?

- `ClientImpl::registered()` ran once when the client joined the bus. It set up a subscription
  pointing at `local.example` with a filter that matches any `Calculator`.
- On the first drain after both objects were registered, Sen populated `calculators_.list` with a
  reference to `calc1`.
- Each cycle, `ClientImpl::update()` reads the frozen list, picks the first calculator, and posts an
  async `add` call.
- The call travels through the kernel's queue, executes in `CalculatorImpl::addImpl` one or two
  cycles later, and the result comes back via the callback.

---

## Next steps

- **[Understanding Sen: A Mental Model](../users_guide/mental_model.md)** — a deeper look at why
  the async model works the way it does.
- **[Working with Objects](../howto_guides/objects.md)** — full reference for subscriptions,
  callbacks, events, and property flags.
- **[Interests and Filtering](../users_guide/sql.md)** — how to write more precise interest queries
  to match only specific objects.
