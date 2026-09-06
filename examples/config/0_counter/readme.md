# My counter

> **Prerequisites:** none. This is the first package, built step by step in the
[Getting Started guide](https://airbus.github.io/sen/latest/getting_started/first_package.html).

This package demonstrates the minimal structure of a Sen package: a single STL file that declares
one class, and one C++ implementation class. It is intentionally small so you can focus on the
mechanics of writing and wiring a Sen package rather than on business logic.

The `Counter` class has:

- a `value` property that increments automatically,
- a `step` property, set once at construction, that says how much it increments by,
- a `hello` method that prints a greeting, and
- a `valueIsDivisibleByTen` event fired whenever `value` becomes divisible by 10.

## Interface

```rust title="counter.stl"
--8<-- "snippets/examples/packages/my_counter/stl/my_counter/counter.stl"
```

## Implementation

=== "_counter.h_"

    ```{ .c++ .annotate title="counter.h" }
    --8<-- "snippets/examples/packages/my_counter/src/counter.h"
    ```

    1. The generated header. Sen writes `CounterBase` from `counter.stl`, carrying the properties,
       the event and the method you declared there.
    2. Your implementation inherits from the generated base, which handles serialisation,
       subscriptions and dispatch.
    3. Deletes the copy and move constructors.
    4. Called once per cycle, during the update stage. This is where the next state is computed.
    5. The base class declares `helloImpl` pure virtual, so it has to be implemented here.

=== "_counter.cpp_"

    ```{ .c++ .annotate title="counter.cpp" }
    --8<-- "snippets/examples/packages/my_counter/src/counter.cpp"
    ```

    1. `getValue()` and `getStep()` read the current buffer, the snapshot frozen before the cycle
       began. `setNextValue()` writes the next one, which everyone sees once Sen commits.
    2. Fires the `valueIsDivisibleByTen` event. Events are buffered like property changes and
       delivered after the commit.
    3. Registers `CounterImpl` so the kernel can instantiate it. Leave it out and the build still
       succeeds, but the kernel stops at startup saying it cannot find the type.

## CMakeLists.txt explained

```cmake
add_sen_package(
  TARGET my_counter                      # Name of the CMake and runtime target
  MAINTAINER "..."                       # Who owns this package
  VERSION "0.0.1"                        # Semantic version embedded in metadata
  DESCRIPTION "Hello Sen tutorial"       # Free text
  SOURCES src/counter.h src/counter.cpp  # C++ files implementing the package logic
  STL_FILES stl/my_counter/counter.stl   # STL interface file; Sen generates C++ from this
)
```

`add_sen_package` creates a shared library that Sen loads at runtime. `STL_FILES` triggers code
generation: the Sen compiler reads the STL file and produces the `CounterBase` C++ class. `SOURCES`
lists the hand-written C++ files compiled alongside the generated code.

## How to run it

```yaml title="config.yaml"
--8<-- "snippets/examples/packages/my_counter/config.yaml"
```

Run the package with:

```shell
sen run packages/my_counter/config.yaml
```

This opens a Sen shell with the `myCounter` object published on the `local.counters` bus. You can
interact with it using:

```text
local.counters.myCounter.print
local.counters.myCounter.hello
```

`print` shows the current property values. `hello` calls the method and prints a greeting to the
terminal running the Sen process.
