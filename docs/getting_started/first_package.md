![Screenshot](../assets/images/package_light.svg#only-light){: style="width:200px; float: right;"}
![Screenshot](../assets/images/package_dark.svg#only-dark){: style="width:200px; float: right;"}

# Create your first package

**What this page is:** a walk through every file `sen package init` generates, and what each line in
it is for. It is the reference to come back to when you want to know why something is in your
package, rather than a narrative to follow once.

**If you have done the tutorials**, you have already built one of these.
[Tutorial 1](../tutorials/hello_sen.md) walks the same ground as a story, with a smaller class and a
running shell at the end. This page is the same territory laid out as reference: one section per
generated file, in the order the build uses them.

**Prerequisites:** Sen installed and the activate script sourced, so that `sen` is on your `PATH`
and `SEN_PREFIX` is exported. See [Getting Sen](install.md).

## The generated layout

Ask Sen to create the skeleton for a package called "my_package", containing a class called
"MyClass".

```sh
sen package init my_package --class MyClass
```

Inspect the contents of the newly-created folder:

```{ .shell .annotate }
  my_package
      ├── CMakeLists.txt # (1)!
      ├── config.yaml # (2)!
      ├── src # (3)!
      │   ├── my_class.cpp
      │   └── my_class.h
      └── stl # (4)!
          └── my_package
              ├── basic_types.stl
              └── my_class.stl # (5)!
```

1. Tells CMake how to build your package.
2. Tells the Sen kernel how to use your package.
3. The implementation of your package.
4. Contains the interface of your package.
5. The class you implement.

## `CMakeLists.txt`: how the package builds

```{ .cmake .annotate }
cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(my_package_project VERSION 0.0.1 LANGUAGES CXX C)

if(DEFINED ENV{SEN_PREFIX}) # (1)!
   list(APPEND CMAKE_PREFIX_PATH "$ENV{SEN_PREFIX}/cmake") # (2)!
endif()

find_package(sen REQUIRED)

add_sen_package( # (3)!
  TARGET
    my_package
  MAINTAINER
    "<your name goes here>" # (4)!
  VERSION
    "0.0.1"   # (5)!
  DESCRIPTION
    "<package description goes here>"
  SOURCES
    src/my_class.h
    src/my_class.cpp
  STL_FILES
    stl/my_package/my_class.stl
    stl/my_package/basic_types.stl
)
```

1. The `SEN_PREFIX` environment variable is exported by the `activate` script the
   installer writes. See [Install](install.md).
2. This enables CMake to find the Sen package below.
3. This function becomes automatically accessible once Sen is found.
4. Replace the placeholders here and in `DESCRIPTION` with your own details. Not mandatory,
   but helpful if you redistribute the package.
5. You can also use the CMake project version here.

## `stl/my_package/my_class.stl`: the interface

This file defines a class with some properties, methods and events. A class can have several
implementations, though this example provides one. You could also import an STL file from another
repository or software asset and have this package implement it, in which case there is no STL file
to write here.

```{ .rust .annotate }
import "stl/my_package/basic_types.stl" // (1)!

package my_package;  // (2)!

class MyClass
{
  var prop1 : string       [static];  // (3)!
  var prop2 : StructOfInts [writable];  // (4)!
  var prop3 : MyVariant    [writable, confirmed];  // (5)!
  var prop4 : Vec2;  // (6)!
  var prop5 : i32          [writable];  // (7)!

  // this method returns a + b
  fn addNumbers(a: i32, b: i32) -> i32;

  // this method returns message
  fn echo(message: string) -> string;

  // change some property
  fn changeProps();

  // fired when something happened
  event somethingHappened();

  // fired when something else happened
  event somethingElseHappened(arg: i32) [confirmed];
}
```

1. Brings in all the types defined in the `basic_types.stl` file (`MyVariant`, `StructOfInts` and
   `Vec2`).
2. Defines the namespace for the types defined in this file.
3. Once defined for a given instance, does not change.
4. Can be set by external callers. Sen generates a setter that's visible from the outside. This
   property goes over UDP.
5. Similar to the previous property, but this one goes over TCP.
6. This property is read-only from the outside. Its setter is generated on the base class, so your
   own implementation can change it, but it is not on the interface other objects hold.
7. A plain counter. The `update()` implementation below increments it on every cycle.

## `src/my_class.h`: the header

A header file for your class is not strictly needed (everything could go in a CPP file, but this
keeps it tidy).

```{ .c++ .annotate }
#pragma once

#include "stl/my_package/my_class.stl.h" // (1)!

// sen
#include "sen/kernel/component_api.h"

namespace my_package
{

class MyClassImpl: public MyClassBase // (2)!
{
public:
  SEN_NOCOPY_NOMOVE(MyClassImpl) // (3)!

public:
  using MyClassBase::MyClassBase;
  ~MyClassImpl() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override; // (4)!

protected: // (5)!
  int32_t addNumbersImpl(int32_t a, int32_t b) override;
  std::string echoImpl(const std::string& message) override;
  void changePropsImpl() override;
};

}  // namespace my_package
```

1. For every STL file Sen will generate the equivalent C++ header.
2. `MyClassBase` is generated by Sen. It contains helper functions and all the glue code.
   [Understanding the generated code](../howto_guides/generated_code.md) goes through what is in
   there and why.
3. This is a helper macro found in the Sen core library. It just disables the copy and move
   operations.
4. This function shows how to evolve the state of your object.
5. The methods are pure virtual in the parent class, so you must implement them.

## `src/my_class.cpp`: the implementation

```{ .c++ .annotate }
#include "my_class.h"

namespace my_package
{

void MyClassImpl::update(sen::kernel::RunApi& /*runApi*/)
{
  setNextProp5(getProp5() + 1);  // here goes your update logic
}

int32_t MyClassImpl::addNumbersImpl(int32_t a, int32_t b)
{
  return a + b;
}

std::string MyClassImpl::echoImpl(const std::string& message)
{
  return message;
}

void MyClassImpl::changePropsImpl()
{
  Vec2 val = getProp4();
  val.x += 0.5f;        // simply make some changes
  val.y += 1.5;         // to a property, to see the effect
  setNextProp4(val);
}

SEN_EXPORT_CLASS(MyClassImpl) // (1)!

}  // namespace my_package
```

1. This exports the class implementation, so users can tell Sen to load the package and instantiate
   `MyClassImpl`s.

## `config.yaml`: the run configuration

```{ .yaml .annotate }
load:
  - name: shell # (1)!
    group: 2    # (2)!
    open: [local.example]  # (3)!

build:
  - name: myComponent # (4)!
    group: 3
    freqHz: 30
    imports:
      - my_package # (5)!
    objects:
      - class: my_package.MyClassImpl # (6)!
        name: myObject
        bus: local.example # (7)!
        prop1: some value # (8)!
```

1. Load the shell, so there is something to look at.
2. The shell runs in group 2, and your component in group 3.
3. Automatically open this bus to see the created objects, so you do not have to open it by hand.
4. This is the name of the component that Sen will build for us.
5. Import your package so Sen can discover your implementation and instantiate your class.
6. The name of the type that provides the implementation, defined in `my_class.cpp`.
7. Your object will be published to this bus, which is why the shell auto-opens it.
8. `prop1` needs a value because it is static, and static properties require an initial value.

## Build and run

To build and run, follow the instructions `sen package init` printed:

```sh
# compile
cmake -S . -B build && cmake --build build

# tell the loader where the package is: bash or zsh
export LD_LIBRARY_PATH="$(pwd)/build/bin:$LD_LIBRARY_PATH"

# run
sen run config.yaml
```

The tool prints the equivalent for fish (`set -xa LD_LIBRARY_PATH $(pwd)/build/bin`) and for
PowerShell on Windows (`$env:PATH = "$PWD\build\bin;$env:PATH"`).

Pointing the loader at `build/bin` is what running from the build tree looks like, and every package
in this repository runs that way. Taking the package anywhere else is a separate step:
`add_sen_package` writes no install rule, so you write your own for the library and for the
generated headers. [Creating your own Conan package](../howto_guides/creating_conan_packages.md)
walks through that, including how to get the generated headers out of `GEN_HDR_FILES`.

From this point you should be able to use the `shell` to inspect and interact with your object.

![Screenshot](https://raw.githubusercontent.com/airbus/sen/refs/heads/docs-assets/listing_objects.gif){: style="width:1200px"}

Stop the kernel with the `shutdown` command.

!!! note "Exit codes"

    When the Sen executable finishes without error it returns zero, and prints a smiling face. If it
    detects an error it can handle, it returns non-zero and prints a frowning one.

## Next

This is the reference route through [Getting started](index.md). If you have not seen Sen run yet,
the [tutorials](../tutorials/index.md) cover the same ground as a story.

The terms this page used without defining them, such as `[static]`, `[writable]`, `[confirmed]`,
buses, groups and `setNext`, are explained in the manual. [Main
concepts](../users_guide/main_concepts.md) covers properties, buses and quality of service; [the
mental model](../users_guide/mental_model.md) explains why a setter is called `setNext`; and [the
Sen Type Language](../users_guide/stl.md) is the reference for everything inside an `.stl` file.
