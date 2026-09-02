# Deferred methods example

This example shows how the deferred methods work. The idea is to model a `Manager` who will receive
the workload from _clients_ and forward it along all the available `Workers` published in
a bus called `workersBus`. If there are no available workers, the manager will perform the task.

## Interface

The `fibonacci` package contains the `fibonacci.stl` file, which holds two interface classes:
`Manager` and `Worker`. `Manager` inherits from `Worker` for allowing him to behave as a worker as
well.

```rust
--8<-- "snippets/examples/packages/fibonacci/stl/fibonacci.stl"
```

## Implementation

To mark methods as deferred, we need to tell the Sen code generator that we don't want to (or can't)
provide a result straight away. To do so, we specify our code generation settings:

The `codegen_settings.json` file is passed to the Sen code generator via the `CODEGEN_SETTINGS`
option in `add_sen_package`. It is a JSON object keyed by class name under `classes`, each listing
the methods to change. Here `fibonacci.Worker` lists `computeFibonacci` under `deferredMethods`.

```json
--8<-- "snippets/examples/packages/fibonacci/src/codegen_settings.json"
```

This will force us to implement a method that, instead of returning a value, receives an
std::promise that we can set whenever we want. In the case of the regular worker, we do the
following:

```c++
--8<-- "snippets/examples/packages/fibonacci/src/fibonacci.cpp:worker"
```

In the case of the manager, our implementation is a bit different:

```c++
--8<-- "snippets/examples/packages/fibonacci/src/fibonacci.cpp:forward"
```

Here we have a list of workers. If we don't find any worker we perform the job on our own.
Otherwise, we select a random worker from the list and call its `computeFibonacci` function. When we
get the result, we set the value of the promise we made to the user. Note that we need to
`std::move` the promise into the callback so that we can use it from within that context.

In Sen, each `Component` runs in an execution thread. If the Workers are defined in the same
component they will share the same thread and won't be able to do work in parallel. In this example,
each `Worker` lives in a different component. Same thing with the `Manager`. Each implementation of
`computeFibonacciImpl` sleeps for five seconds before computing, to make this threading behavior
visible. The `computeFibonacci` function itself has no side effects.

NOTE: Since `Manager` extends `Worker`, a query for workers on the `workersBus` would match the
manager itself. The manager's interest excludes it by name, so it never appears in its own worker
list.

```yaml title="Configuration file"
--8<-- "snippets/examples/config/12_fibonacci/1_fibonacci.yaml"
```

## How to run it

```shell
sen run config/12_fibonacci/1_fibonacci.yaml
```

This will open a shell and tell Sen to instantiate the implementations in the `my.tutorial` bus.

You ask the `Manager` for computing _n_ Fibonacci number by:

```text
my.tutorial.fibManager.computeFibonacci 8
```

Note that the potential of this example is to ask the Manager for as many petitions as desired,
while crosschecking that the workload is constantly being distributed and simultaneously performed
by the different workers without delaying the workload distribution.

You can also ask one of the workers directly. The configuration publishes five of them, `fibWorkerA`
through `fibWorkerE`:

```text
my.tutorial.fibWorkerA.computeFibonacci 8
```

## Testing package internals with `TEST_TARGET`

By default, Sen packages compile with hidden symbol visibility, so unit tests cannot link directly
against the package and call internal functions. The `TEST_TARGET` option creates an additional
STATIC library with all symbols exposed, which you can link your test executables against.

The fibonacci package uses this to expose and test the pure `computeFibonacci` algorithm
independently of the runtime. In the package's `CMakeLists.txt`:

```cmake
--8<-- "snippets/examples/packages/fibonacci/CMakeLists.txt:package"
```

`TEST_TARGET` names the static library, `fibonacci_test_lib`. The unit test suite is a separate
target, registered with `add_sen_unit_test_suite` and linked against that static library rather than
against the shared package.

Any modifications to the target (e.g. `target_include_directories`) must be applied to
`fibonacci_obj`, the intermediate OBJECT target, not to `fibonacci` or `fibonacci_test_lib` directly
(see the CMake integration page of the manual for details).
