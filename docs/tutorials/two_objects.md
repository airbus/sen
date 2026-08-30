# Tutorial 2: Two objects talking

In the previous tutorial you built a single object and inspected it in the shell. Now you will build
two objects that find each other at runtime and communicate: one calls a method on the other, and
handles the result asynchronously.

**What you'll learn:**

- How objects discover each other using subscriptions and interests
- How to call a method on a remote object and handle the result
- Why method calls in Sen are always asynchronous, and how to work with that
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
2. Other objects declare an **interest** (a filter) that describes what they are looking for (by
   class, name, or both).
3. Sen gives them a **`Subscription<T>`**, a live list that is automatically populated with
   matching objects as they appear and cleaned up as they disappear.

This is reactive. You don't poll for objects. The list updates itself during the drain stage of each
cycle. Your code just reads from the list whenever it needs to.

!!! note "Subscriptions are live"
    If a new `Calculator` appears on the bus mid-execution (because a new component started), it
    will appear in your subscription list on the next drain. If it disappears (component stopped),
    it will be removed. You never hold a dangling pointer to a dead object.

---

## Step 1: Define the interface

This tutorial walks through the `calculators` example, which lives in Sen's own source tree. Every
listing below is the real file, so what you read here is what compiles.

**You need the examples built.** They are off by default, so configure with
`-o "sen/*:with_examples=True"`; see
[Building Sen from source](../howto_guides/building_from_source.md). If you installed Sen from a
release you do not have them, and you can still read this tutorial straight through.

The interface lives in `examples/packages/calculators/stl/calculator.stl`:

```rust
--8<-- "examples/packages/calculators/stl/calculator.stl"
```

`current` stores the running total. It is read-only to other objects, so only the calculator itself
can change it. `calcBus` is the bus name where the client looks for calculators, and it is set in
the YAML configuration.

The interface carries a little more than this tutorial uses: `divideByCurrent`, and a `model`
property that each implementation must be given in the configuration.

---

## Step 2: Implement the Calculator

An implementation inherits from the generated `CalculatorBase` and fills in the method bodies. The
example ships two, `CasioCalculator` and `FaultyCalculator`; here is the first.

```{ .cpp .annotate }
--8<-- "examples/packages/calculators/src/casio_calculator.cpp"
```

1. Stage the new value. It won't be visible until commit, but the return value is delivered to the
   caller immediately when their next drain processes the method result.
2. Dividing by zero does both things Sen offers. It emits an event, which suits an error that
   subscribers may want to observe, and it throws, which is how the caller finds out: Sen catches
   the exception, wraps it in a `MethodResult` and delivers it to the caller's callback. Returning
   a value the caller cannot tell apart from a real answer is the one option it does not take.

---

## Step 3: Implement the Client

This is where the interesting part happens. The `Client` needs to:

1. Find `Calculator` objects on a bus
2. Call methods on them and handle results

```{ .cpp .annotate }
--8<-- "examples/packages/calculators/src/client.cpp"
```

1. `registered()` is called once after the object is added to the bus. This is the right place to
   set up subscriptions: the object is live and the API is available.
2. **This must be a member variable**, not a local. `Subscription<T>` must stay alive as long as
   you want to receive updates. If it goes out of scope, the list is destroyed and callbacks stop.
3. `api.selectAllFrom<CalculatorInterface>(getCalcBus())` matches *all* objects that implement
   `CalculatorInterface`. Subscribe to the generated interface, not to `CasioCalculator`, because a
   calculator in another process has no implementation on your side to name.
4. Always guard against an empty list. Objects can disappear between cycles.
5. `list.front()` returns a reference to the first matching object. The reference is valid for this
   entire update cycle (it was frozen during drain).
6. The callback receives a `MethodResult`, holding either the value or an exception. `float32_t` is
   what `f32` means in the generated header, so the types match exactly. It fires during the drain
   stage of a future cycle, once the result has been committed by the calculator.

---

## Step 4: The async call timeline

This is the most important thing to internalize:

```text
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
    result arrives 1–2 cycles later depending on scheduling. This is by design. It is what makes
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
    Sen ensures the callback is only invoked while your object is still alive: if your object
    is unregistered before the result arrives, the callback is silently dropped.

---

## Step 5: Configure and run

```{ .yaml .annotate }
--8<-- "examples/config/1_calculators/4_calculators_client.yaml"
```

1. Tells the client which bus to search for calculators. Must match the bus where `calc1` is
   published.

With the examples built, run this from the `examples/` directory of your Sen checkout:

```sh
sen run config/1_calculators/4_calculators_client.yaml
```

The shell opens on `my.tutorial`. Asking the client to run gives you the print from the callback:

```text
sen:host/4_calculators_client> my.tutorial.client1.useCalculator
add(3, 4) = 7.000000
```

You can also drive the calculator yourself. Note that reading a property prints it as
`- <name>: <value>`, and that `current` only changes once the call has been through a cycle:

```text
sen:host/4_calculators_client> my.tutorial.calc1.getCurrent
- current: 0.000000

sen:host/4_calculators_client> my.tutorial.calc1.add 10, 5
15.000000

sen:host/4_calculators_client> my.tutorial.calc1.getCurrent
- current: 15.000000
```

`info` prints the whole interface, which is the quickest way to see what the generator made of your
STL:

```text
sen:host/4_calculators_client> info my.tutorial.calc1

  OBJECT calc1 [id 0x3f2803f4]

  CLASS calculators.Calculator

  DESCRIPTION
    A simple calculator that can add and divide numbers.

  PROPERTIES
    [string] model   st-rw-multicast The model of the calculator
    [f32]    current dy-ro-multicast The last result, or what is currently pre..

  METHODS
    add             Returns "a + b" and sets the last result to it.
    addWithCurrent  Returns "a + R" where R is was the last result.
    divide          Returns "a / b" and sets the last result to it.
    divideByCurrent Returns "a / R" where R is was the last result. If R is 0,..

  EVENTS
    divisionByZero multicast Emitted when there's an attempt to divide by zero.
```

The flags are the ones from [Tutorial 1](hello_sen.md#step-5-explore-the-object), so `current` reads
as dynamic and read-only. The `DESCRIPTION` block is new here: it is the comment above `class
Calculator` in the STL, which is reason enough to write one.

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
  pointing at `my.tutorial` with a filter that matches any `Calculator`.
- On the first drain after both objects were registered, Sen populated `calculators_.list` with a
  reference to `calc1`.
- When someone calls `useCalculator` on the client, `useCalculatorImpl()` reads the frozen list,
  picks the first calculator, and posts an async `add` call.
- The call travels through the kernel's queue, executes in `CasioCalculator::addImpl` one or two
  cycles later, and the result comes back via the callback.

---

## Next steps

- **[Understanding Sen: a mental model](../users_guide/mental_model.md)**: a deeper look at why
  the async model works the way it does.
- **[Working with objects](../howto_guides/objects.md)**: full reference for subscriptions,
  callbacks, events, and property flags.
- **[Interests and Filtering](../users_guide/sql.md)**: how to write more precise interest queries
  to match only specific objects.
