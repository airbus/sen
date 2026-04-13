# My Counter

> **Prerequisites:** none - this is the introductory package used in the [Getting Started](../../docs/getting_started.md) guide.

This package demonstrates the minimal structure of a Sen package: a single STL file that declares one class, and one
C++ implementation class. It is intentionally small so you can focus on the mechanics of writing and wiring a Sen
package rather than on business logic.

The `Counter` class has:

- a `count` property that increments automatically,
- a `hello` method that prints a greeting, and
- a `valueIsDivisibleByTen` event fired whenever count becomes divisible by 10.

## Interface

```rust title="counter.stl"
--8<-- "snippets/examples/packages/my_counter/stl/my_counter/counter.stl"
```

## Implementation

=== "_counter.h_"

    ```c++ title="counter.h"
    --8<-- "snippets/examples/packages/my_counter/src/counter.h"
    ```

=== "_counter.cpp_"

    ```c++ title="counter.cpp"
    --8<-- "snippets/examples/packages/my_counter/src/counter.cpp"
    ```

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

`add_sen_package` creates a shared library that Sen loads at runtime. `STL_FILES` triggers code generation: the Sen
compiler reads the STL file and produces the `CounterBase` C++ class. `SOURCES` lists the hand-written C++ files
compiled alongside the generated code.

## How to run it

```yaml title="config.yaml"
--8<-- "snippets/examples/packages/my_counter/config.yaml"
```

Run the package with:

```
sen run examples/packages/my_counter/config.yaml
```

This opens a Sen shell with the `myCounter` object published on the `local.counters` bus. You can interact
with it using:

```
local.counters.myCounter.print
local.counters.myCounter.hello
```

`print` shows the current property values. `hello` calls the method and prints a greeting to the terminal running
the Sen process.
