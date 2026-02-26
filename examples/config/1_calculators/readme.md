# Calculators example

This example illustrates how you can create a package that holds two implementations of a class
defined in STL.

## Interface

This package provides the definition of a calculator.

```rust title="calculator.stl"
--8<-- "snippets/examples/packages/calculators/stl/calculator.stl"
```

## Implementation

It also provides two implementations for the `Calculator` class.

=== "_casio_calculator.cpp_"

    ```c++ title="Implementation of a calculator"
    --8<-- "snippets/examples/packages/calculators/src/casio_calculator.cpp"
    ```

=== "_faulty_calculator.cpp_"

    ```c++ title="Another implementation of a calculator"
    --8<-- "snippets/examples/packages/calculators/src/faulty_calculator.cpp"
    ```

## How to run it

Let's define what we want to run in our Sen kernel.

```yaml title="Configuration file"
--8<-- "snippets/examples/config/1_calculators/1_calculators.yaml"
```

To run it, let's call `sen run`:

```shell
sen run config/1_calculators/1_calculators.yaml
```

This will open a shell and tell Sen to instantiate the two implementations in the `my.tutorial` bus.

You can interact with the objects by doing commands such as:

```shell
info my.tutorial.goodCalc

my.tutorial.goodCalc.add 2, 2

my.tutorial.goodCalc.print

my.tutorial.goodCalc.addWithCurrent 2

my.tutorial.goodCalc.getCurrent

my.tutorial.goodCalc.divideByCurrent 12

my.tutorial.goodCalc.divide 4, 2

my.tutorial.goodCalc.addWithCurrent -2

my.tutorial.goodCalc.divideByCurrent 4

my.tutorial.bsCalc.add 2, 2

my.tutorial.bsCalc.add 2, 2
```

## Running it over the network

We can run it over the network using the eth component. This is the same as the first example, but
you will need to start two processes.

First run:

```
sen run config/1_calculators/2_calculators_eth.yaml
```

Then, in another terminal or command prompt, run:

```
sen shell
```

In this new Sen instance, open the bus where we should find our objects:

```
open my.tutorial
```

You should be able to work with the objects as if you were on the same process.

## Using the explorer

You can also run it using the explorer to see and interact with the objects in a more graphical way.

```
sen run config/1_calculators/3_calculators_exp.yaml
```
