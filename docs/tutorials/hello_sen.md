# Tutorial 1: Hello Sen

In this tutorial you will build the simplest possible Sen application: a single object that updates a
property each cycle and exposes a method. By the end, you will have a running kernel, a live object
visible in the shell, and a clear picture of how Sen's pieces fit together.

**What you'll learn:**

- The STL → code generation → C++ → YAML → run workflow
- How to define properties and methods in STL
- How to implement a generated base class
- How to stage property updates with `setNext` and why that matters
- How to use the shell to inspect a live object

**Prerequisites:** Sen installed and on your `PATH`, basic C++ knowledge.

---

## What we're building

A simple counter object that increments a value every update cycle and exposes an `add` method.
Nothing exciting — but it touches every part of the workflow in the most direct way possible.

---

## Step 1: Create the package skeleton

Sen can generate the folder structure for you:

```sh
sen package init my_counter --class Counter
```

This creates:

```{ .shell .annotate }
my_counter/
    ├── CMakeLists.txt  # (1)!
    ├── config.yaml     # (2)!
    ├── src/
    │   ├── counter.cpp
    │   └── counter.h
    └── stl/
        └── my_counter/
            ├── basic_types.stl
            └── counter.stl  # (3)!
```

1. Tells CMake how to build the package. Uses `add_sen_package()` — a function Sen provides.
2. A ready-to-run configuration file that instantiates the object and opens a shell.
3. The interface definition for our `Counter` class. We'll edit this next.

---

## Step 2: Define the interface

Open `stl/my_counter/counter.stl` and replace its contents with this:

```{ .rust .annotate }
package my_counter;  // (1)!

class Counter
{
  // The current value of the counter (2)!.
  var value : i32;

  // How much should the counter increase each cycle (3)!.
  var step    : i32 [static];

  // Returns a greeting message (4)!.
  fn hello() -> string [const];

  // Emitted each time the current value is divisible by 10 (5)!.
  event valueIsDivisibleByTen(newValue: i32);
}
```

1. Declares the namespace. All types defined here belong to `my_counter`.
2. A dynamic, read-only property. Our object updates it each cycle. Other components can read it.
3. A static property — its value is set at construction and never changes. We'll set it in `config.yaml`.
4. A method that returns a greeting message. It is marked as `const` so it cannot change the object.
5. An event fired whenever the value is divisible by 10. Any component can subscribe to this.

!!! note "Static means constant per instance"
    "Static" in STL does not mean shared across instances. It means: set once at construction, then
    immutable. Every instance of `Counter` can have a different `step` value — you just can't change
    it after the object is registered.

---

## Step 3: Implement the class

### The header

Edit `src/counter.h`:

```{ .cpp .annotate }
#pragma once

#include "stl/my_counter/counter.stl.h"  // (1)!

namespace my_counter
{

class CounterImpl : public CounterBase  // (2)!
{
public:
  SEN_NOCOPY_NOMOVE(CounterImpl)  // (3)!

  using CounterBase::CounterBase;
  ~CounterImpl() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override;  // (4)!

protected:
  std::string helloImpl() const override;  // (5)!
};

}  // namespace my_counter
```

1. The code generator produces this header from your STL file. It contains `CounterBase` and all the
   generated plumbing.
2. You always inherit from the generated base class. It handles serialization, subscriptions, and
   method dispatch — you just fill in the logic.
3. A helper macro that deletes the copy and move constructors.
4. Called once per cycle during the *update* stage. This is where you compute your next state.
5. The generated base class declares `helloImpl` as a pure virtual method. You must implement it.

### The implementation

Edit `src/counter.cpp`:

```{ .cpp .annotate }
#include "counter.h"

namespace my_counter
{

void CounterImpl::update(sen::kernel::RunApi& /*runApi*/)
{
  setNextValue(getValue() + getStep()); // (1)!

  if (getNextValue() % 10 == 0)
  {
    valueIsDivisibleByTen(getNextValue());  // (2)!
  }
}

std::string CounterImpl::helloImpl() const
{
  return "Hello from Sen! My current value is: " + std::to_string(getValue());
}

SEN_EXPORT_CLASS(CounterImpl)  // (3)!

}  // namespace my_counter
```

1. `getValue()` and `getStep()` read from the *current* buffer — the frozen snapshot that Sen prepared during the
   drain stage. These values will not change during this update cycle. `setNextValue()` writes to the *next* buffer.
   The new value is **not** visible yet. It becomes visible to all components after Sen commits the outputs.
2. Fires the `valueIsDivisibleByTen` event with the new value. Like property changes, events are buffered and
   delivered after commit.
3. This macro registers `CounterImpl` as a class that Sen's kernel can instantiate. Without it, your
   class is invisible to the configuration system — no error at build time, just nothing shows up.

!!! abstract "Why `setNext` instead of `set`?"

    Sen uses double-buffering. During the update stage, every component sees the same frozen snapshot
    of the world. If `setNextValue()` wrote directly to the live value, another component reading
    `getValue()` on the same cycle might see a half-updated state. By writing to a separate "next"
    buffer, Sen guarantees that all components see a fully consistent world — the flip happens
    atomically during commit.

    Think of it like frames in a film: you render to the back buffer, then swap.

---

## Step 4: Configure and run

Edit `config.yaml` so it looks like this:

```{ .yaml .annotate }
load:
  - name: shell   # (1)!
    group: 2
    open: [local.counters]  # (2)!

build:
  - name: counterComponent
    freqHz: 2      # (3)!
    group: 3
    imports: [my_counter]  # (4)!
    objects:
      - class: my_counter.CounterImpl  # (5)!
        name: myCounter
        step: 5         # (6)!
        bus: local.counters  # (7)!
```

1. Load the shell component so we can interact with the running system.
2. Automatically open this bus in the shell so we can see objects without typing `open` manually.
3. The component (and all its objects) will call `update()` twice per second.
4. Tell Sen to load our package so it can find `CounterImpl`.
5. The fully-qualified class name: `<package>.<class>`.
6. Initial value for the `step` static property. Required — static properties must have a value.
7. The bus where our object will be published. Must match what the shell opens.

Now build and run:

```sh
cmake -S . -B build && cmake --build build
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/build/bin
sen run config.yaml
```

---

## Step 5: Explore the object

The shell opens automatically. Try these commands:

```sh
# List everything on the bus
ls

# Inspect the object — see all properties, methods and events
info local.counters.myCounter

# Read the current property value
local.counters.myCounter.getValue

# Wait a second, read it again — you'll see it has incremented
local.counters.myCounter.getValue

# Call the hello method
local.counters.myCounter.hello

# Shut down cleanly
shutdown
```

!!! tip
    The shell has tab-completion. Type `local.` and press `Tab` to see available buses and objects.

---

## What just happened?

Here is the execution in plain English:

1. **`sen run config.yaml`** starts the kernel. It reads the config, loads the `shell` component
   (group 2), then builds the `counterComponent` (group 3). Groups ensure the shell is ready before
   the component starts.

2. **`CounterImpl` is instantiated** with `step = 5` and registered on the `local.counters` bus.
   The shell sees it immediately because it has the bus open.

3. **Every 500 ms** (2 Hz), the kernel runs the drain → update → commit cycle for `counterComponent`:
   - **Drain**: Sen delivers any pending method calls and property changes.
   - **Update**: Sen calls `CounterImpl::update()`. You read `getValue()` (frozen snapshot),
     compute the next value, and call `setNextValue()` (writes to back buffer). The event is also
     staged.
   - **Commit**: Sen atomically flips the buffers. The new `current` value is now visible to
     everyone. The `valueIsDivisibleByTen` event is delivered to any subscribers.

4. **When you called `hello`** in the shell, the kernel queued the method call. On the next drain,
   `helloImpl()` executed, and returned a string. Your shell received the result via
   a callback.

---

## Next steps

- **[Tutorial 2: Two Objects Talking](two_objects.md)** — learn how objects discover each other and
  make async method calls.
- **[Understanding Sen: A Mental Model](../users_guide/mental_model.md)** — a deeper explanation of
  the drain-update-commit cycle and how Sen differs from other systems.
- **[Working with Objects](../howto_guides/objects.md)** — the full how-to reference for
  properties, methods, events and subscriptions.
