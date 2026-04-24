# Python Interpreter examples

> **Prerequisites:** [1 - Calculators](../1_calculators/readme.md) (basic objects). Sub-examples 5 and 6 also use [4 - School](../4_school/readme.md).

Here you can see how we load and execute Python scripts inside a Sen process.

First make sure that you have the `PYTHONPATH` environment variable correctly set to point to our
example scripts:

For bash:

```
export PYTHONPATH=$PYTHONPATH;$(pwd)/config/10_python/scripts
```

For fish:

```
set -xa PYTHONPATH $(pwd)/config/10_python/scripts
```

**PYTHONPATH setup**: The Python scripts import `sen` (the Sen Python binding) and optionally generated type modules. Before running these scripts standalone, ensure `PYTHONPATH` includes the directory containing the `sen` module and any generated Python packages. When using `sen run`, Sen automatically extends `PYTHONPATH` with the paths of Python targets listed in `DEPS` inside `sen_generate_yaml`.

## Hello Python

In this example we just show how to get a Python interpreter running inside a Sen component.

Run it with:

```
sen run config/10_python/1_python_hello.yaml
```

=== "Python script"
    ```python title="hello_python.py"
    --8<-- "snippets/examples/config/10_python/scripts/hello_python.py"
    ```

=== "Configuration file"
    ```yaml title="1_python_hello.yaml"
    --8<-- "snippets/examples/config/10_python/1_python_hello.yaml"
    ```

## Inspecting objects

In this example we can see how to fetch objects coming from other components and print their properties.

Run it with:

```
sen run config/10_python/2_python_inspect_objects.yaml
```

This example contains the following files:

=== "Python script"
    ```python title="inspecting_objects.py"
    --8<-- "snippets/examples/config/10_python/scripts/inspecting_objects.py"
    ```

=== "Configuration file"
    ```yaml title="2_python_inspect_objects.yaml"
    --8<-- "snippets/examples/config/10_python/2_python_inspect_objects.yaml"
    ```

## Creating objects

In this example we see how to create and publish objects using Python.

Run it with:

```
sen run config/10_python/3_python_create_objects.yaml
```

This example contains the following files:

=== "Python script"
    ```python title="creating_objects.py"
    --8<-- "snippets/examples/config/10_python/scripts/creating_objects.py"
    ```

=== "Configuration file"
    ```yaml title="3_python_create_objects.yaml"
    --8<-- "snippets/examples/config/10_python/3_python_create_objects.yaml"
    ```

## Interacting with objects

Here we can see how to call methods on objects coming from other components.

Run it with:

```
sen run config/10_python/4_python_interact_with_objects.yaml
```

This example contains the following files:

=== "Python script"
    ```python title="interacting_with_objects.py"
    --8<-- "snippets/examples/config/10_python/scripts/interacting_with_objects.py"
    ```

=== "Configuration file"
    ```yaml title="4_python_interact_with_objects.yaml"
    --8<-- "snippets/examples/config/10_python/4_python_interact_with_objects.yaml"
    ```

## Reacting to events and changes

Here we can see how to execute Python code when events are produced or properties change.

It relies on the "school" example to get some activity going.

Run it with:

```
sen run config/10_python/5_python_react_to_events.yaml
```

This example contains the following files:

=== "Python script"
    ```python title="reacting_to_events_and_changes.py"
    --8<-- "snippets/examples/config/10_python/scripts/reacting_to_events_and_changes.py"
    ```

=== "Configuration file"
    ```yaml title="5_python_react_to_events.yaml"
    --8<-- "snippets/examples/config/10_python/5_python_react_to_events.yaml"
    ```

## Using the interpreter from other components

Here we publish the interpreter object and can use it from within our shell component.

```
sen run config/10_python/6_python_interpreter.yaml
```

This example contains the following files:

```yaml title="6_python_interpreter.yaml"
--8<-- "snippets/examples/config/10_python/6_python_interpreter.yaml"
```

If you open the shell you can use the interpreter with something like this:

```
sen:enrique-debian/6_python_interpreter> local.py.interpreter.eval "2+2"
"4"
```

The `eval` function evaluates an expression and returns the result.

```
sen:enrique-debian/6_python_interpreter> local.py.interpreter.exec "x = 2"
```

The `exec` function executes statements.

```
sen:enrique-debian/6_python_interpreter> local.py.interpreter.eval "2+x"
"4"
```

The interpreter holds an internal state, so you can use it in your evaluations.
