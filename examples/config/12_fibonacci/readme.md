# Deferred Methods Example

This example shows how the deferred methods work. The idea is to model a `Manager` who will receive
the workload from _clients_ and forward it along all the available `Workers` published in
a bus called `factoryBus`. If there are no available workers, the manager will perform the task.

## Interface

The `fibonacci` package contains the `fibonacci.stl` file, which holds two interface classes:
`Manager` and `Worker`. `Manager` inherits from `Worker` for allowing him to behave as a worker as
well.

```rust
--8<-- "snippets/examples/packages/fibonacci/stl/fibonacci.stl"
```

## Implementation

To mark methods as deferred, we need to tell the Sen code generator that we don't want to (or can't) provide
a result straight away. To do so, we specify our code generation settings:

```json
--8<-- "snippets/examples/packages/fibonacci/src/codegen_settings.json"
```

This will force us to implement a method that, instead of returning a value, receives an std::promise
that we can set whenever we want. In the case of the regular worker, we do the following:

```c++
void FibonacciWorker::computeFibonacciImpl(uint32_t n, std::promise<uint64_t>&& promise) override
{
  promise.set_value(fibonacci(n));
}
```

In the case of the manager, our implementation is a bit different:

```c++
void FibonacciManager::computeFibonacciImpl(uint32_t n, std::promise<uint64_t>&& promise) override
{
  if (workers_->list.getObjects().empty())
  {
    promise.set_value(fibonacci(n)); // No workers available, manager is performing the task
  }
  else
  {
    selectRandomWorker().computeFibonacci(
      n, {this, [p = std::move(promise)](const auto& result) mutable { p.set_value(result.getValue()); }});
  }
}
```

Here we have a list of workers. If we don't find any worker we perform the job on our own. Otherwise, we select
a random worker from the list and call its `computeFibonacci` function. When we get the result, we set the
value of the promise we made to the user. Note that we need to `std::move` the promise into the callback so that
we can use it from within that context.

In Sen, each `Component` runs in an execution thread. If the Workers are defined in the same component they
will share the same thread and won't be able to do work in parallel. In this example, each `Worker` lives in
a different component. Same thing with the `Manager`. The implementation of our `fibonacci(n)` function
adds some artificial sleep time to better show this threading behavior.

NOTE: Since the `Manager` inherits from `Worker` class, when looking for the available workers published
in the `workersBus`, it finds itself as well. This is taken into account when selecting workers.

```yaml title="Configuration file"
--8<-- "snippets/examples/config/12_fibonacci/1_fibonacci.yaml"
```

## How to run it

```
sen run config/12_fibonacci/1_fibonacci.yaml
```

This will open a shell and tell Sen to instantiate the implementations in the `my.tutorial` bus.

You ask the `Manager` for computing _n_ Fibonacci number by:

```
my.tutorial.fibManager.computeFibonacci 8
```

Note that the potential of this example is to ask the Manager for as many petitions as desired,
while crosschecking that the workload is constantly being distributed and simultaneously performed
by the different workers without delaying the workload distribution.

You ask the `Worker` for computing _n_ Fibonacci number by:

```
my.tutorial.fibWorker.computeFibonacci 8
```
