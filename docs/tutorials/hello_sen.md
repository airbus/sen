# Tutorial 1: Hello Sen

In this tutorial you will build the simplest possible Sen application: a single object that updates
a property each cycle and exposes a method. By the end, you will have a running
[kernel](../users_guide/glossary.md#kernel), a live object visible in the shell, and a clear
picture of how Sen's pieces fit together.

**What you'll learn:**

- The STL → code generation → C++ → YAML → run workflow
- How to define properties and methods in STL
- How to implement a generated base class
- How to stage property updates with `setNext` and why that matters
- How to use the shell to inspect a live object

**Prerequisites:** Sen installed, basic C++ knowledge, and the activation script sourced in the
shell you are about to work in:

```sh
. ~/.sen/current/activate           # bash / zsh
source ~/.sen/current/activate.fish # fish
```

Having `sen` on your `PATH` is not enough. The script also exports `SEN_PREFIX`, which is what lets
`find_package(sen REQUIRED)` succeed in Step 4. Without it, `cmake` fails at configure time with
"Could not find a package configuration file provided by sen". See [Getting
Sen](../getting_started/install.md).

---

## What we're building

A simple counter object that increments a value every drain-update-commit cycle and exposes a
`hello` method.
Nothing exciting, but it touches every part of the workflow in the most direct way possible.

---

## Step 1: Create the package skeleton

Sen can generate the folder structure for you:

```sh
sen package init my_counter --class Counter
cd my_counter
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

1. Tells CMake how to build the package. Uses `add_sen_package()`, a function Sen provides.
2. A ready-to-run configuration file that instantiates the object and opens a shell.
3. The interface definition for your `Counter` class. You will edit this next.

---

## Step 2: Define the interface

Open `stl/my_counter/counter.stl` and replace its contents with this:

```rust
--8<-- "examples/packages/my_counter/stl/my_counter/counter.stl"
```

`package my_counter` declares the namespace, so everything defined here belongs to it.

`value` is dynamic and read-only: your object updates it each cycle, and other components can read
it. `step` is static, so it is set at construction and never changes, and you will set it in
`config.yaml`. `hello()` is marked `const`, so it cannot change the object.
`valueIsDivisibleByTen` is an event, and any component can subscribe to it.

!!! note "Static means constant per instance"
    "Static" in STL does not mean shared across instances. It means: set once at construction, then
    immutable. Every instance of `Counter` can have a different `step` value; you just can't change
    it after the object is registered.

---

## Step 3: Implement the class

### The header

Edit `src/counter.h`:

```{ .cpp .annotate }
--8<-- "examples/packages/my_counter/src/counter.h"
```

1. The code generator produces this header from your STL file. It contains `CounterBase` and all the
   generated plumbing.
2. You always inherit from the generated base class. It handles serialization, subscriptions, and
   method dispatch. You just fill in the logic.
3. A helper macro that deletes the copy and move constructors.
4. Called once per cycle during the *update* stage. This is where you compute your next state.
5. The generated base class declares `helloImpl` as a pure virtual method. You must implement it.

### The implementation

Edit `src/counter.cpp`:

```{ .cpp .annotate }
--8<-- "examples/packages/my_counter/src/counter.cpp"
```

1. `getValue()` and `getStep()` read from the *current* buffer, the frozen snapshot that Sen
   prepared during the drain stage (the first of the cycle's three stages, explained in [What just
   happened?](#what-just-happened) below). These values will not change during this update cycle.
   `setNextValue()` writes to the *next* buffer. The new value is **not** visible yet. It becomes
   visible to all components after Sen commits the outputs. `getNextValue()` reads that next buffer
   back, which is how the line below tests the value it has just written instead of last cycle's.
2. Fires the `valueIsDivisibleByTen` event with the new value. Like property changes, events are
   buffered and delivered after commit.
3. This macro registers `CounterImpl` as a class that Sen's kernel can instantiate. Without it there
   is no build error, but the kernel stops at startup with `could not find type
   'my_counter.CounterImpl' in any of the imported libraries`, naming the symbol it looked for.

!!! abstract "Why `setNext` instead of `set`?"

    Sen uses double-buffering. During the update stage, every component sees the same frozen
    snapshot of the world. If `setNextValue()` wrote directly to the live value, another component
    reading `getValue()` on the same cycle might see a half-updated state. By writing to a separate
    "next" buffer, Sen guarantees that all components see a fully consistent world. The flip happens
    atomically during commit.

    The [double-buffer analogy](../users_guide/mental_model.md#the-double-buffer-analogy) in the
    manual works this through.

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
4. Tell Sen to load your package so it can find `CounterImpl`.
5. The C++ class you registered with `SEN_EXPORT_CLASS`, qualified by its package. This is not the
   STL class name: the shell below shows the same object as `my_counter.Counter`, the class it
   implements.
6. Initial value for the `step` static property. Required: static properties must have a value.
7. The bus where your object will be published. Must match what the shell opens.

Now build and run:

```sh
cmake -S . -B build && cmake --build build
export LD_LIBRARY_PATH="$(pwd)/build/bin:$LD_LIBRARY_PATH"
sen run config.yaml
```

---

## Step 5: Explore the object

The shell opens automatically. Its prompt shows the host and the configuration you started, so it
looks like `sen:host/config>`. Start with `ls`, which lists everything the shell can see:

```text
sen:host/config> ls
  ┬
  └─┬local [session]
    ├─┬counters [bus]
    │ └──myCounter [my_counter.Counter]
    ├──shell [~]
    ├──kernel [~]
    └──log [~]
```

`local` is the session, `counters` is the bus we opened, and `myCounter` is your object with its
class beside it. The three entries marked `[~]` are the components: the shell itself, the kernel and
the log.

`info` prints the interface, which is the fastest way to confirm the generator understood your STL:

```text
sen:host/config> info local.counters.myCounter

  OBJECT myCounter [id 0x68c1c76c]

  CLASS my_counter.Counter

  PROPERTIES
    [i32] value dy-ro-multicast The current value of the counter.
    [i32] step  st-rw-multicast How much should the counter increase each cycle.

  METHODS
    hello Returns a greeting message.

  EVENTS
    valueIsDivisibleByTen multicast Emitted each time the current value is div..
```

The flags say what each property is: `dy` or `st` for dynamic or static, and `ro` or `rw` for
read-only or read-write. So `value` is the dynamic, read-only one we update each cycle and `step` is
the static one we set in the configuration, which is what the STL declared. Descriptions come
straight from your comments, truncated to fit the terminal.

Read-only and read-write are about everyone else, not about you: you always set your own properties
with `setNext<Prop>()`. `value` is a plain `var`, so nobody outside can write it. `step` is
`[static]`, and that is what makes it settable from the configuration. Declaring a dynamic property
`[writable]` opens it to other objects in the same way.

Now read the properties. A getter prints as `- <name>: <value>`:

```text
sen:host/config> local.counters.myCounter.getValue
- value: 125

sen:host/config> local.counters.myCounter.getValue
- value: 185

sen:host/config> local.counters.myCounter.getStep
- step: 5
```

Your numbers will differ from these, and that is the point: at 2 Hz with `step: 5` the value climbs
by ten every second for as long as the kernel runs, so what you see depends on how long it has been
up and how fast you type. `step` does not move, because it is static.

That coupling is deliberate here and fine for a counter. A value that should advance per second, and
not per cycle, has to read the time instead, which
[the execution model](../users_guide/execution_model.md#the-time-a-component-sees) shows how to do.

Calling the method returns a string:

```text
sen:host/config> local.counters.myCounter.hello
"Hello from Sen! My current value is: 250"
```

Finally, `shutdown` stops the kernel:

```text
sen:host/config> shutdown
shutting down...
bye ☺
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

3. **Every 500 ms** (2 Hz), the kernel runs the drain-update-commit cycle for `counterComponent`:
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

- **[Tutorial 2: Two objects talking](two_objects.md)**: learn how objects discover each other and
  make async method calls.
- **[Understanding Sen: a mental model](../users_guide/mental_model.md)**: a deeper explanation of
  the drain-update-commit cycle and how Sen differs from other systems.
- **[Working with objects](../howto_guides/objects.md)**: the full how-to reference for
  properties, methods, events and subscriptions.
