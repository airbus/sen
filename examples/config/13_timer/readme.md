# Checked Properties Example

> **Prerequisites:** [1 - Calculators](../1_calculators/readme.md) (basic properties and functions).

This example shows how to use checked properties.

## Interface

We model a `Timer` that holds two writable properties. One is called `program` ad the other is called `state`.

```rust
--8<-- "snippets/examples/packages/timer/stl/timer.stl"
```

This property called `program` is "checked". We only accept sets to `program` when `state`is `off`.

## Implementation

We mark the `program` property as "checked" using the code generation settings:

```json
--8<-- "snippets/examples/packages/timer/src/codegen_settings.json"
```

Now that the property is "checked", we need to implement the following function:

```c++
bool programAcceptsSet(sen::Duration /*val*/) const override
{
  if (getState() == RunningState::off)
  {
    std::cout << "Timer is off! Switch it on to start running." << std::endl;
    return true;
  }

  std::cout << "Timer is running! cannot change the program." << std::endl;
  return false;
}
```

## How to run it

```
sen run config/13_timer/1_timer.yaml
```

This will open a shell and tell Sen to instantiate the implementations in the `my.tutorial` bus.

You can set the initial timer seconds by:

```
my.tutorial.timer.setNextProgram "20 s"
```

Note that the timer will not start running until it is On. You can turn on / off the `Timer` by:

```
my.tutorial.timer.setNextState "on"
```

Once the timer is running, it is not possible to edit its `program` property until it reaches the end.
You would need to turn the timer off or wait until it times out.
