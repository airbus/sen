![Screenshot](../assets/images/python_light.svg){: style="width:200px; float: right;"}

# The Python Component

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
| getBusName(name)      | sen.Bus        | Returns an object representing a bus. Use the "session.bus" format. |
| open(query)           | sen.ObjectList | Returns a list containing the objects matching the query.           |
| make(type, name)      | sen.Object     | Creates a new object. Constructor args go last and are keyed.       |
| waitUntil(cond, time) | Boolean        | Holds the execution until the condition is met (timeout is opt.).   |

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
    object.on < EventName > (callback)
    object.on < PropertyName > Changed(callback)
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
component as follows:

```yaml title="hello_python.yaml"
# Load the Python interpreter and run the hello_python module
load:
  - name: py
    group: 3
    freqHz: 1
    module: hello_python  # don't include the .py extension here
```

This will instantiate the interpreter in a component that will run your module.

Example module:

```python title="hello_python.py"
def run():
  print(f"Python: run")
  print(f"Python: the config is: {sen.api.config}")
  print(f"Python: the app name is: {sen.api.appName}")


def update():
  print(f"Python: update (current time: {sen.api.time})")


def stop():
  print(f"Python: stop called")
```

NOTE: Remember to add the folder where your scripts are located to the `PYTHONPATH` environment
variable.

This would print:

```
Python: run
Python: update (time: ...)
Python: update (time: ...)
Python: update (time: ...)
...
Python: stop called
```

## Inspecting Objects

Use the `sen.api.open(query)` to get access to objects. This returns a list that is automatically
updated. You can also define callbacks to react on objects being added or removed.

The list can also be iterated as a normal Python sequence.

For example:

```python title="inspecting_objects.py"
import sen

# to store the objects that we want to inspect
list = None


def run():
  global list  # refer to the global variable defined above

  list = sen.api.open("SELECT * FROM local.kernel")  # open it

  # register some callbacks to show changes in the list
  list.onAdded(lambda obj: print(f'Python: object added {obj}'))
  list.onRemoved(lambda obj: print(f'Python: object removed {obj}'))


def update():
  # refer to the global variable defined above
  global list

  print(f"Python: printing the list at: {sen.api.time})")
  print(list)

  print("Python: iterating over the objects in the list:")
  for obj in list:
    print(f"Python: * object {obj.name}")
    print(f"Python:   - class: {obj.className}")
    print(f"Python:   - id:    {obj.id}")
    print(f"Python:   - time:  {obj.lastCommitTime}")
```

All objects define the `name`, `className`, `id` and `lastCommitTime` as read-only properties. The
next example shows a more complete representation of accessing object members.

## Interacting with objects

You can call C++ methods on objects from Python. For example:

```python title="interacting_with_objects.py"
import sen

# to store the object
obj = None


def run():
  global obj  # refer to the global variable defined above
  obj = sen.api.open("SELECT * FROM local.shell WHERE name = \"shell_impl\"")


def update():
  global obj  # refer to the global variable defined above
  print("Python: update")

  # if the object is present, do something with it
  if len(obj) != 0:
    print("Python: interacting with the object")
    obj[0].info("i16")  # print the info of the i16 type
    obj[0].ls()  # list the current objects in the terminal
    obj[0].history()  # show the current history

    print("Python: asking the process to shut down")
    obj[0].shutdown()  # trigger the process shutdown
```

If you need to process the return value of a method call (which are asynchronous), you can pass a
callback. For example:

```python
obj.addNumbers(2, 2, lambda result: print(f"the result is {result}"))
```

## Reacting to events and property changes

As in C++, you can attach a callback to react to events and/or property changes. For example:

```python title="reacting_to_events_and_changes.py"
import sen

# to store the objects
teachers = None


def teacherDetected(teacher):
  teacher.onStressLevelPeaked(lambda args: print(f"Python: {teacher.name} stress level peaking to {args}"))
  teacher.onStatusChanged(lambda: print(f"Python: {teacher.name} status changed to {teacher.status}"))


def run():
  global teachers  # refer to the global variable defined above

  print("Python: run")

  # select the object and install some callbacks
  teachers = sen.api.open("SELECT school.Teacher FROM school.primary")
  teachers.onAdded(lambda obj: teacherDetected(obj))
```

## Creating and publishing objects

You can import your Sen packages and instantiate objects from Python. For example:

```python title="creating_objects.py"
import sen

myObject = testBus = None

def run():
  global myObject, testBus  # refer to the globals defined above

  type = {
    "entityKind": 1,
    "domain": 2,
    "countryCode": 198,
    "category": 1,
    "subcategory": 3,
    "specific": 0,
    "extra": 0
  }

  id = {
    "entityNumber": 1,
    "federateIdentifier": {
      "siteID": 1,
      "applicationID": 1
    }
  }

  print(f"Python: creating and publishing the object")
  myObject = sen.api.make("aircrafts.DummyAircraft", "myAircraft", entityType=type, alternateEntityType=type, entityIdentifier = id)
  testBus = sen.api.getBus("my.tutorial")
  testBus.add(myObject)

def update():
  print(myObject)

def stop():
  global testBus, myObject  # refer to the globals defined above

  print("Python: deleting the object")
  testBus.remove(myObject)
  myObject = None
  testBus = None
```

In this case we had to import the `my_package` Sen package using our configuration file. It looked
like this:

```yaml title="creating_objects.yaml"
# Load the shell, the Python interpreter and run the creating_objects module
load:
  - name: shell
    group: 2
    open: [ "local.test" ]
  - name: py
    group: 3
    freqHz: 1
    module: creating_objects
    imports: [ my_package ]
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

```
Python: enum 'sen.my_package.MyEnum.first' has the value 1
```

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
can manipulate sen objects in order to verify complex behaviours, making this a powerful tool for
automated integration testing.

The use of pytest-bdd allows the users to automate project requirements testing in a flexible way
using the Gherkin language.

## Using Python from your package or component

The Python component will create an "Interpreter" object and publish it in a given bus if you define
the "bus" configuration option.

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

```
./sen run test.yaml
    _________  __
   / __/ __/ |/ /          compiler GNU-11.4.0 [mode: debug]
  _\ \/ __/    /           revision 57a86bb1d7ebcd781bb034d73642dde198e69fde
 /___/___/_/|_/   0.0.1    branch   refs/heads/master [modified]
▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄

sen:enrique-ubuntu/test> local.py.interpreter.exec "c = 2"
sen:enrique-ubuntu/test> local.py.interpreter.eval "c"
"2"
sen:enrique-ubuntu/test> local.py.interpreter.exec "c = c + 2"
sen:enrique-ubuntu/test> local.py.interpreter.eval "c"
"4"
sen:enrique-ubuntu/test> local.py.interpreter.exec "import my_module"
sen:enrique-ubuntu/test> local.py.interpreter.eval "my_module.add(2,2)"
"4"
```

With this approach, you are also enabled to call the interpreter if you want to delegate some
functionality into Python. Just find the interpreter in the selected bus, compose your code,
evaluate your expressions and interpret the result.
