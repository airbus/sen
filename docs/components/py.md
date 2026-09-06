![Screenshot](../assets/images/python_light.svg){: style="width:200px; float: right;"}

# The Python component

Sen allows you to use Python by embedding an interpreter and giving you native access to all
objects, sessions and buses. In particular:

- Sen objects will be represented as regular Python objects. You can use properties, react to events
  and call methods.
- You can create queries to look for objects.
- You can import your own Sen C++ packages and directly create, manipulate and publish your own
  objects.
- You can fetch information of the execution context (application name, current time, etc.)
- You can install callbacks for your Python code to be called on certain events or property changes.

One of the key reasons for having Python is to leverage on all its existing libraries and scripting
power to let you implement custom, complex, or highly customizable logic with ease. Examples of this
might be tests ( at unit, integration, behavioral or sub-system level), orchestration logic,
visualization, analysis, ML, etc.

Sen will try to find the following functions in your Python module:

| Name     | Description                                                                              |
| :------- | ---------------------------------------------------------------------------------------- |
| run()    | Called once at the start of the component execution.                                     |
| update() | Called on every iteration. The execution rate is defined as `freqHz` in the config file. |
| stop()   | Called once at the end of the execution.                                                 |

The `sen.api` attribute is a Python object that allows you to interact with Sen. It defines the
following members:

| Name                  | Type           | Description                                                         |
| :-------------------- | :------------- | ------------------------------------------------------------------- |
| time                  | timedelta      | The current time.                                                   |
| defaultTimeout        | timedelta      | Default timeout used in waitUntil(), if none given.                 |
| appName               | string         | The name of the application where the script is being executed.     |
| config                | dictionary     | Configuration passed to the component to parametrize your module.   |
| getBus(name)          | sen.Bus        | Returns an object representing a bus. Use the "session.bus" format. |
| open(query)           | sen.ObjectList | Returns a list containing the objects matching the query.           |
| make(type, name)      | sen.Object     | Creates a new object. Constructor args go last and are keyed.       |
| waitUntil(cond, time) | Boolean        | Holds the execution until the condition is met (timeout is opt.).   |
| requestKernelStop()   | None           | Asks the kernel to shut down.                                       |

The `sen.Bus` class has the following members:

| Name           | Description                                              |
| :------------- | -------------------------------------------------------- |
| add(object)    | Registers an object. Does nothing if already registered. |
| remove(object) | Unregisters an object. Does nothing if not present.      |

The `sen.ObjectList` is a regular Python sequence with the following extra members:

| Name                   | Description                                                     |
| :--------------------- | --------------------------------------------------------------- |
| onAdded(callback)      | Registers a function to be called when a new object gets added. |
| onRemoved(callback)    | Registers a function to be called when an object gets removed.  |
| waitUntilEmpty()       | Holds the execution until the list is empty.                    |
| waitUntilNotEmpty()    | Holds the execution until the list is not empty.                |
| waitUntilSizeIs(count) | Holds the execution until the list size is equal to count.      |

The `sen.Object` class contains all the properties and methods of the corresponding class. In
addition, you can register callbacks to react to events and property changes:

```python
object.on<EventName>(callback)
object.on<PropertyName>Changed(callback)
```

Independently of their class, all `sen.Object` instances have the following members:

| Name           | Type      | Description                                           |
| :------------- | :-------- | ----------------------------------------------------- |
| name           | string    | The name of this instance, as provided by its owner.  |
| localName      | string    | The name of this instance, as seen by this component. |
| id             | integral  | Unique identifier.                                    |
| lastCommitTime | timedelta | Time point of the last commit made on this object.    |
| className      | string    | The name of the class.                                |

We will now go over a set of simple examples to illustrate the API.

## Examples

You need to define your Python module, which consists of a main .py file. Then you load the `py`
[component](../users_guide/glossary.md#component) as follows:

```yaml title="1_python_hello.yaml"
--8<-- "examples/config/10_python/1_python_hello.yaml"
```

This will instantiate the interpreter in a component that will run your module. `module` names the
Python module, so it carries no `.py` extension. The `include` pulls in the shell, which is how the
shipped examples avoid repeating it in every configuration.

Example module:

```python title="hello_python.py"
--8<-- "examples/config/10_python/scripts/hello_python.py:script"
```

NOTE: Remember to add the folder where your scripts are located to the `PYTHONPATH` environment
variable.

This would print:

```text
Python: run
Python: the config is: ...
Python: the app name is: ...
Python: update (current time: ...)
Python: update (current time: ...)
...
Python: stop called
```

## Inspecting objects

Use the `sen.api.open(query)` to get access to objects. This returns a list that is automatically
updated. You can also define callbacks to react on objects being added or removed.

The list can also be iterated as a normal Python sequence.

For example:

```python title="inspecting_objects.py"
--8<-- "examples/config/10_python/scripts/inspecting_objects.py:script"
```

All objects define the `name`, `className`, `id` and `lastCommitTime` as read-only properties. The
next example shows a more complete representation of accessing object members.

## Interacting with objects

You can call C++ methods on objects from Python. For example:

```python title="interacting_with_objects.py"
--8<-- "examples/config/10_python/scripts/interacting_with_objects.py:script"
```

If you need to process the return value of a method call (which is asynchronous), you can pass a
callback. For example:

```python
obj.addNumbers(2, 2, lambda result: print(f"the result is {result}"))
```

## Reacting to events and property changes

As in C++, you can attach a callback to react to events and/or property changes. For example:

```python title="reacting_to_events_and_changes.py"
--8<-- "examples/config/10_python/scripts/reacting_to_events_and_changes.py:script"
```

## Creating and publishing objects

You can import your Sen packages and instantiate objects from Python. For example:

```python title="creating_objects.py"
--8<-- "examples/config/10_python/scripts/creating_objects.py:script"
```

In this case we had to import the `my_package` Sen [package](../users_guide/glossary.md#package)
using your configuration file. It looked like this:

```yaml title="3_python_create_objects.yaml"
--8<-- "examples/config/10_python/3_python_create_objects.yaml"
```

## Using enumerations

You can use native enumerations in Python, as Sen automagically defines them for the Sen packages
that you import. To do so, you need to use the `import sen.<package>` directive in Python. For
example, say that we have a package called `my_package` that defines an enumeration type called
`MyEnum` with the corresponding value:

```python
import sen.my_package


def run():
    print(f"Python: enum 'sen.my_package.MyEnum.first' has the value '{sen.my_package.MyEnum.first}'")
```

This would print:

```text
Python: enum 'sen.my_package.MyEnum.first' has the value 'first'
```

The value is the enumerator's name, not its ordinal. Sen converts that name back when the value
crosses into a Sen object, so passing an integer in its place fails rather than being interpreted.

## Making your script fully sequential

During testing, you might be in the need of performing fully sequential calls.

If you need to ensure that the method call is completed. You can set the `syncCalls` flag to `True`
and treat it as a regular function call.

```python
sen.api.syncCalls = True

result = obj.addNumbers(2, 2)
print(f"the result is {result}")
```

If you need to wait until a certain condition is achieved, use the
`sen.api.waitUntil(condition, timeout = 0)` function. This function will hold the script execution
until the condition you specify (as a function) is fulfilled.

The `timeout` is a duration that, when elapsed, will fail the `waitUntil` call, making it return
`False`.

## Testing with python

You can use python for testing your sen objects in a flexible manner. Using pytest framework, you
can manipulate sen objects in order to verify complex behaviors, making this a powerful tool for
automated integration testing.

The use of pytest-bdd allows the users to automate project requirements testing in a flexible way
using the Gherkin language.

## Using Python from your package or component

The Python component will create an "Interpreter" object and publish it in a given
[bus](../users_guide/glossary.md#bus) if you define the "bus" configuration option.

It provides the following interface:

```rust title="python_interpreter.stl"
--8<-- "snippets/python_interpreter.stl"
```

For example, you can now call Python from the shell:

```yaml title="test.yaml"
load:
  - name: shell
    open: [local.py]
  - name: py
    group: 3
    freqHz: 30
    bus: local.py
```

```shell
./sen run test.yaml
    _________  __
   / __/ __/ |/ /          compiler GNU-11.4.0 [mode: debug]
  _\ \/ __/    /           revision 57a86bb1d7ebcd781bb034d73642dde198e69fde
 /___/___/_/|_/   0.0.1    branch   refs/heads/master [modified]
▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄

sen:host/test> local.py.interpreter.exec "c = 2"
sen:host/test> local.py.interpreter.eval "c"
"2"
sen:host/test> local.py.interpreter.exec "c = c + 2"
sen:host/test> local.py.interpreter.eval "c"
"4"
sen:host/test> local.py.interpreter.exec "import my_module"
sen:host/test> local.py.interpreter.eval "my_module.add(2,2)"
"4"
```

With this approach, you are also enabled to call the interpreter if you want to delegate some
functionality into Python. Just find the interpreter in the selected bus, compose your code,
evaluate your expressions and interpret the result.
