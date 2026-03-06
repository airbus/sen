# Writing a Component (advanced - internal to Sen)

The reality is that you are likely going to write more packages than components, but this tutorial
is fundamental to understand how Sen works, and therefore it provides the knowledge you will need to
design and implement your packages.

## Getting it started

Let's create our first component and get it to run in a Sen process.

We can ask Sen to create a skeleton for our component by doing:

```console
$ sen package init-component MyComponent
$ tree my_component/
my_component/
├── CMakeLists.txt
└── src
   └── component.cpp
1 directory, 2 files
```

If we open the CMakeLists.txt file and fill in our info, we get:

```cmake title="CMakeLists.txt"
add_sen_package(
        TARGET my_component
        MAINTAINER "John Doe (johndoe@mail.com)"
        VERSION "0.0.1"
        DESCRIPTION "This is my first component"
        SOURCES lang/component.cpp
        IS_COMPONENT
)
```

The `add_sen_component` function is a CMake helper that will create a shared library target
containing all the required meta information. This information also includes the git version, branch
and status, compiler and compilation flags, etc.

Add the CMakeLists.txt to your CMake project. We will now have a target named `my_component` that
will be built as a shared object.

Let's have a look at the component.cpp file:

```c++ title="component.cpp" linenums="1"
#include "sen/kernel/component.h"

#include <iostream>
#include <thread>

struct MyComponent: public sen::kernel::Component /*(1)*/
{
  sen::kernel::FuncResult run(sen::kernel::RunApi& api /*(2)*/) override
  {
    std::cout << "MyComponent: started running\n";

    while (!api.stopRequested())
    {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      std::cout << "MyComponent: hello\n";
    }

    std::cout << "MyComponent: finished\n";
    return done();
  }
};

SEN_COMPONENT(MyComponent) //-> (3)
```

1. :man_raising_hand: The `sen::kernel::Component` class also has other methods that you can
   implement if you want to prepare your component before running.

2. :man_raising_hand: The `sen::kernel::RunApi`, like the name indicates, is the runtime API that
   allows your component to interact with Sen. It only has a few (but very powerful) methods. We
   will get to know them later on.

3. :man_raising_hand: This macro is important because this is what lets Sen know that you are
   exporting the `MyComponent` class. The only thing that this macro does is to create a C function
   that the Sen kernel can find and use to instantiate your class.

You can see that we just print the fact that we started running and wait for Sen to notify us that
we should stop via `#!cpp api.stopRequested()`.

To get our component running, we first need to write our configuration file `config.yaml` (Sen uses
YAML. See [this page](https://spacelift.io/blog/yaml) for more info). We can write the following:

```yaml title="config.yaml"
load:
  - name: my_component
    group: 2
```

This says: "Please run my_component in group 2". Let's try it.

```console
$ sen run config.yaml

MyComponent: started running
MyComponent: hello
MyComponent: hello
MyComponent: hello
```

And it keeps printing "hello" forever (you need to kill the process with ++ctrl+c++).

We are able to run, but there is no way for telling the kernel that it should stop. As you will see,
there are some built-in components that can help us interact with the kernel in an easy way.

## Getting it stopped

Sen comes with a *shell* component that provides a rich command-line interface to interact with the
kernel our components and our objects.

As we don't want our component to pollute our terminal with those "hello" messages, let's remove
that print:

```cpp title="removing prints from component.cpp" linenums="12"
while (!api.stopRequested())
{
  std::this_thread::sleep_for(std::chrono::seconds(1));
}
```

Now let's add the *shell* to our configuration file.

```yaml title="config.yaml"
load:
  # first, load the shell
  - name: shell
    group: 2
    open: [ local.kernel ]

  # then, load our component
  - name: my_component
    group: 3
```

If you now run it, you should see that the shell starts and then our component gets to run. We can
now stop the kernel by using the `shutdown` command and see that our component does in fact shut
down and prints the correct message.

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/fix/images/shutdown.gif){: style="width:1200px"}

When the Sen executable finishes without error, it prints a :smiley: and returns zero. If it detects
and is able to handle an error it will print a :slightly_frowning_face: and returns non-zero. This
is independent of any component.

If we do an `ls` in our *shell* we can see the objects that are currently published.

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/fix/images/component_ls.gif){: style="width:1200px"}

You can see that the kernel itself is publishing some objects in a bus called "local.kernel". In
that bus, it publishes objects that represent the running components. There we can see our component
as "my_component". We can also see the "shell" component, that we also loaded, and a "kernel"
component that is always added by the Sen kernel itself.

Now let's inspect the object that was published to represent our component and see some info about
it.

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/fix/images/print_component_info.gif){: style="width:1200px"}

Here we can see the meta information that gets automatically baked into our component binary. Some
of it comes from our CMakeLists.txt, and some gets added by the build environment used when
compiling it.

You can also see that there's a `config` field. It contains the information that the kernel is using
to run your component. You can customize it, but for now we are only setting the `group` to 2, so
that our component starts after the *shell*.

## Adding parameters

Processes need inputs to be able to do something. In general, there are two sources of inputs: the
ones you get at start-up and the ones you get at run-time. Here we will address the former and get
to parametrize our component.

In Sen, we have a type-safe environment. In order to be type-safe you need to tell Sen which types
do you want to work with. You do it by using the Sen Type Language (STL). Once you define your
types, then the user can define values that Sen will parse out of the config file and provide them
to you in your language's native representation.

Let's imagine that our component is in charge of managing the information about users of some TV
streaming service. For each user we need to store their basic info, address and subscription plan.
Let's define our first STL file:

```rust title="stl/configuration.stl"
package example;

// The address of a user
struct Address
{
  city   : string, // Name of the city
  street : string, // Full street name
  number : u32     // Number of the building (1)
}

// Subscription plan (2)
enum Plan : u8
{
  basic,   // Only one HD screen
  premium, // One 4K screen and unlimited HD
  gold     // Unlimited 4K content
}

// Information about our users
struct User
{
  name    : string,   // Full name
  points  : f32,      // Customer points (3)
  plan    : Plan,     // The subscription plan
  address : Address,  // Where does it lives
  since   : Duration  // Since when is subscribed
}

// A list of users
sequence<User> UsersList;

// Our component configuration
struct Configuration
{
  serviceName : string,   // Name of our service
  users       : UsersList // Our users
}
```

1. In STL, integral types are `u8`, `u16`, `i16`, `u32`, `i32`, `u64` and `i64`. The `u` is for
   "unsigned integer" and the `i` is for "signed integer". The number refers to the bits that are
   used to represent the values.

2. The `: u8` after the enum type name represents the integral type that will be used to store the
   values.

3. In STL, floating-point types are `f32` and `f64`. The `f` is for "floating point" and the number
   refers to the bits that are used to represent the values. Floating points are always represented
   in the IEEE 754 standard.

Now that we have defined our data model, let's tell Sen to include it into our component's code by
updating our CMake file:

```cmake title="CMakeLists.txt"  hl_lines="7"
add_sen_package(
        TARGET my_component
        MAINTAINER "John Doe (johndoe@mail.com)"
        VERSION "0.0.1"
        DESCRIPTION "This is my first component"
        SOURCES lang/component.cpp
        STL_FILES stl/configuration.stl
        IS_COMPONENT
)
```

With this, we now have a generated header that can be used in our `component.cpp` file. The
generated header is named like the STL file, but with a `.h` at the end. In our case it is
`stl/configuration.stl.h`.

Let's have a look at it:

```cpp title="stl/configuration.stl.h"
#ifndef STL_CONFIGURATION_STL_H
#define STL_CONFIGURATION_STL_H

namespace example
{

/// The address of a user
struct Address
{
  std::string city{};    ///<  Name of the city
  std::string street{};  ///<  Full street name
  u32 number{};          ///<  Number of the building
};

/// Subscription plan
enum class Plan: u8
{
  basic = 0,    ///<  Only one HD screen
  premium = 1,  ///<  One 4K screen and unlimited HD
  gold = 2,     ///<  Unlimited 4K content
};

/// Information about our users
struct User
{
  std::string name{};     ///<  Full name
  f32 points{};           ///<  Customer points
  Plan plan{};            ///<  The subscription plan
  Address address{};      ///<  Where does it lives
  sen::Duration since{};  ///<  Since when is subscribed
};

/// Our component configuration
struct Configuration
{
  std::string serviceName{};  ///<  Name of our service
  UsersList users{};          ///<  Our users
};

}  // namespace example

#endif STL_CONFIGURATION_STL_H
```

You can see that the translation between STL and C++ is pretty straight-forward, readable, and it
also contains the documentation (in a format that is compatible with Doxygen).

We can now use these types to get our configuration in our component.

```cpp title="component.cpp" hl_lines="2 13 14 16 17 19 20"
#include "sen/kernel/component.h"
#include "stl/configuration.stl.h"

#include <iostream>
#include <thread>

struct MyComponent: public sen::kernel::Component
{
  sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
  {
    std::cout << "MyComponent: started running\n";

    // to store our configuration
    auto config = sen::toValue<example::Configuration>(api.getConfig());

    // print it
    std::cout << config << "\n";

    while (!api.stopRequested())
    {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "MyComponent: finished\n";
    return done();
  }
};

SEN_COMPONENT(MyComponent)
```

See? no parsing no looking for fields or checking types. You have your data in a native format. And
you can even directly print it!

The piece of code doing the magic is `sen::toValue<example::Configuration>(api.getConfig())`. This
is a type trait template that gets generated and allows us to do things with our types that regular
C++ does not. In this case we are using it to convert the "type-unsafe" data that the kernel gives
us in the form of a variant (via `api.getConfig()`) and extract it into our type.

We haven't yet provided any data to our component, so if we run it, we get the following:

```
MyComponent: started running
  serviceName:
  users:       null
```

Let's change this and provide our component with data. We just need to update the configuration file
and add some:

```yaml title="config.yaml"
load:
  - name: shell
    group: 2
    open: [ local.kernel ]

  - name: my_component
    group: 3
    serviceName: SenFlix
    users:
      - name: John Doe
        points: 5.4
        plan: basic
        since: 10 s
        address:
          city: Ulanbataar
          street: Las Quejas
          number: 69
      - name: Elon Musk
        points: 0.3
        plan: gold
        since: 5 s
        address:
          city: Ciudad Juarez
          street: Los Malandros
          number: 12
```

Now, when we run our component we get the data:

```
MyComponent: started running
  serviceName: SenFlix
  users:
      name:    John Doe
      points:  5.4
      plan:    basic
      address:
        city:   Ulanbataar
        street: Las Quejas
        number: 69
      since:   10 s

      name:    Elon Musk
      points:  0.3
      plan:    gold
      address:
        city:   Ciudad Juarez
        street: Los Malandros
        number: 13
      since:   5 s
```

That's it! Now you know how to parametrize your components and work with complex configuration
parameters without the burden of parsing config files or dealing with type-unsafe constructs.

## The environment

We can tell the kernel to update any input we might have by calling `RunApi::drainInputs()`, and
notify others about our activity by calling `RunApi::commit()`. This also applies to our sources.
Let's see how our component's loop would look like with this:

```cpp title="drain-update-flush loop" hl_lines="3 5"
while (!api.stopRequested())
{
  api.drainInputs();
  // ... maybe do something (and probably sleep for a while) ...
  api.commit();
}
```

But writing those loops in every component is tedious and prone to error. Sen comes with a helper
function that allows you to do the same:

```cpp title="Built-in drain-update-flush loop"
auto func = [](){ /* ... do something */ };
api.execLoop(sen::Duration::fromHertz(1.0), std::move(func));
```

Here you see that the sleep time is given as a time duration (which we construct in this case from a
period in Hz).

The function `func` will be called on every iteration of the loop.

If you don't have anything to do, but to react to the inputs and interactions made by other
components, you can simply do the following:

```cpp
api.execLoop(sen::Duration::fromHertz(1.0));
```

And adding all of this into our example, we end up with a very compact component implementation.

```cpp title="Using the built-in drain-update-flush loop" hl_lines="8"
struct MyComponent: public sen::kernel::Component
{
  sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
  {
    auto bus = api.getSource("local.kernel"); // get the source
    sen::ObjectList<sen::Object> objects;     // create a container
    bus->addSubscriber(sen::Interest::make("SELECT * FROM local.kernel", api.getTypes()),
                       &objects, true); // subscribe to all objects
    return api.execLoop(sen::Duration::fromHertz(1.0));
  }
};
```

The only problem here is that we don't print the objects that we are discovering. Let's add some
code to do that:

```cpp title="Full example, printing discovered objects" hl_lines="13 14"
#include "sen/kernel/component.h"

#include <iostream>

struct MyComponent: public sen::kernel::Component
{
  sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
  {
    auto bus = api.getSource("local.kernel"); // get the source
    sen::ObjectList<sen::Object> objects;     // create a container

    // print discovered objects
    std::ignore = objects.onAdded([](const auto& iterators)
    {
      for (auto itr = iterators.typedBegin; itr != iterators.typedEnd; ++itr)
      {
        std::cout << "\n - got " << (*itr)->getLocalName() << "\n";
      }
    });
    std::ignore = objects.onRemoved([](const auto& iterators)
    {
      for (auto itr = iterators.typedBegin; itr != iterators.typedEnd; ++itr)
      {
        std::cout << "\n - lost " << (*itr)->getLocalName() << "\n";
      }
    });

    bus->addSubscriber(sen::Interest::make("SELECT * FROM local.kernel", api.getTypes()),
                       &objects, true); // subscribe to all objects
    return api.execLoop(sen::Duration::fromHertz(1.0));
  }
};

SEN_COMPONENT(MyComponent)
```

Let's run it:

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/fix/images/listing_objects.gif){: style="width:1200px"}

You can see that we can see some objects. In particular:

| Local Name                                          | Description                |
| --------------------------------------------------- | -------------------------- |
| `my_component.local.kernel.components.my_component` | Represents our component.  |
| `my_component.local.kernel.components.shell`        | The shell component.       |
| `my_component.local.kernel.components.kernel`       | The kernel component.      |
| `my_component.local.kernel.api`                     | Represents the kernel API. |

Notice that all the objects start with the `"my_component."` prefix. This is because we are working
with *Proxy Objects* that represent our view of the system. Those objects are "our copy" of the real
objects that live elsewhere, and they are guaranteed not to change while we are running. They have
the `"my_component."` prefix because Sen created them for us.

The "local" session is special because buses there will never be shared across the process boundary.

## Publishing objects

Objects are owned by components. That means that if you want to publish an object, you need to own
its memory. This can be done using a regular `std::shared_ptr<T>`.

```c++ title="A run function that publishes an object"
sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
{
  auto obj = std::make_shared<MyClass>("myObject"); // create the object
  auto bus = api.getSource("local.myBus");          // get the bus
  bus->add(obj);                                    // publish the object
  result = api.execLoop(defaultShellUpdateFreq);    // execute
  bus->remove(obj);                                 // remove the object
  return result;                                    // done
}
```

You can store your objects on the stack or as a member of your component class. Same with sources.
We just need to be aware of the following:

- The `RunApi::getSource()` function returns a `std::shared_ptr<ObjectSource>` that needs to be kept
  alive during the usage of the source. If there are no more references to our source, it will be
  automatically closed by the kernel.
- After the execution loop, it is advisable to explicitly remove our objects from the sources.

Object names must be unique within the bus in which they are published. Otherwise, an exception will
be raised.
