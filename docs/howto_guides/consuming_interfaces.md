# Use interfaces from an imported Sen-based project.

This guide will focus on how to set up your project to consume interfaces from external packages.
For this guide to be accurate, the package you consume should have followed the instructions
described on the [how to export interfaces in Sen-based projects](exportable_interfaces.md) guide.

## How to consume Sen-based projects as CMake packages

In order to use the interfaces of a project you consume externally, you first need the project
itself. Consuming Sen-based projects is done via CMake. Every Sen-based project will be exported as
a CMake package, and obtaining those packages can be done in several ways: Conan package manager,
ZIP files, CMake installations, etc. The way of obtaining the package is transparent: the only
requirement is that the path to the `-config.cmake` file that every project has is added to the
`CMAKE_PREFIX_PATH`.

Setting the `CMAKE_PREFIX_PATH` depends on the method you choose to consume the package:

- If you consume a Sen-based package via Conan, the `CMAKE_PREFIX_PATH` will be set
  **automatically**.
- If you use ZIP files or CMake installations, you will need to provide the correct path to the
  directory before calling CMake's `find package`.

After ensuring that the project's CMake directory is added to the `CMAKE_PREFIX_PATH` variable, we
can now call

```cmake
find_package(my_project)
```

to add the consumed project to ours. Sen uses an independent CMake file for each dependency, so the
recommendation is to follow the same architecture. You can check Sen's dependencies located in
`cmake/tps/` or the dependencies of any Sen-based project (e.g. `sen_dis_gateway`) under
`cmake/deps` for further detail.

## Use external interfaces inside our project

After importing the external project into ours, we can make use of Sen's CMake tools to use the
interfaces of an externally included project.

### get_external_interfaces function

Sen's CMake utils implement the `get_external_interfaces()` function. This function receives the
name of any Sen-based CMake target with an STL interface, and an installation dir, which should
point to the directory where Sen is installed and being consumed externally:

```cmake
get_external_interfaces(TARGET target_name INSTALLATION_DIR install_dir)
```

- The `TARGET` argument will be the name of the desired CMake target who you want to obtain the
  interfaces from. The target name will stay the same as it is on the original project, preceded by
  its namespace. For example, if I want to obtain Sen's recorder component interfaces, I would call
  get_external_interfaces(TARGET sen::recorder ...).
- The `INSTALLATION_DIR` argument is a variable that points to the actual directory where the
  imported package is located at compile time. If the CMake package of the consumed project was
  generated using the [how to export interfaces in Sen-based projects](exportable_interfaces.md)
  guide, this variable **should** be set to `*project_name*_INSTALL_DIR`. If the variable is not set
  the function call will fail, showing possible solutions of the problem, ultimately leading to
  contact the maintainer of the consumed project.

Calling the `get_external_interfaces` function will add new CMake properties to the target that can
later be used to generate code making use of Sen's CMake generation functions (`sen_generate_cpp`,
`sen_generate_package`, `add_sen_package`, etc.).

### Generating code using external interfaces

After calling `get_external_interfaces`, CMake will add three additional properties to the given
`TARGET` that were not present at the time of building:

- `INSTALL_STL_FILES`: List of paths to each of the component's STL files in the installation dir.
- `INSTALL_BASE_PATH`: Set BASE_PATH when generating the packages originally but adapted to point at
  the installation dir.
- `INSTALL_HLA_FOM_DIRS`: List of paths to each of the component's defined HLA_FOM_DIRS in the
  installation dir.

You can use CMake's `get_target_property` function to fetch the value of these properties into local
variables. With these variables filled, you will be able to call Sen's code generation functions
with the files used to generate code on the original target. This allows replicating the generated
code of the original package, but allowing it to be imported into your project, added as a
dependency of your Sen Packages, etc.

### Importing generated code

If the consumed project followed the
[how to export interfaces in Sen-based projects](exportable_interfaces.md) guide, you will be able
to import the stl files in **exactly the same way** as they are imported in the original project.
This affects to `stl` inclusions (`import stl/sen/components/recorder/recorder.stl`), CPP
(`#include "stl/sen/components/recorder/recorder.stl.h"`) or any language supported by Sen.

Note that you are not only limited to STL interfaces as HLA FOM files can also be exported in the
same way.

## Usage example

In this use case example, I have a component called `Town`. The town component resides in my
project, but it wants to use the interfaces of the already created `school` component, present on
the Sen project. The steps to follow would be:

1 . Import Sen's CMake package by consuming it via Conan (or any other method to obtain packages)
and call `find_package` accordingly.

2 . In your project's CMake files, call the `get_external_interfaces` function with the adequate
arguments:

```cmake
  get_external_interfaces(TARGET sen::school INSTALLATION_DIR ${SEN_INSTALL_DIR})
```

3 . Obtain both the `stl` files list and the `BASE_PATH` with `get_target_property` calls, storing
them in local CMake variables:

```cmake
  get_target_property(school_base_path sen::school INSTALL_BASE_PATH)
  get_target_property(school_stl_files sen::school INSTALL_STL_FILES)
```

4 . Use the locally filled variables to generate the `cpp` code of the `school` interfaces:

```cmake
sen_generate_cpp(TARGET sen_school_interfaces 
                 BASE_PATH ${school_base_path} 
                 STL_FILES ${school_stl_files})
```

NOTE: if the interfaces you are using depend on any other interfaces (e.g., Sen Recorder interface
depends on `sen::db` library interfaces), you need to generate them with a Sen code generation
function that allows adding dependencies with the `DEPS` argument. This two functions are
`add_sen_component` and `add_sen_package`.

5 . After generating the code of the interfaces, you can use the obtained target
`sen_school_interfaces` inside your Sen packages:

```cmake
add_sen_package(TARGET town
                SOURCES ${town_sources}
                BASE_PATH ${town_source_dir}
                STL_FILES ${town_source_dir}/stl/sen/components/town/town.stl
                DEPS sen_school_interfaces)
```

With the interfaces as a dependency, you will be able to import their STL files in any file of your
Sen package.

- In STL files:

```cpp
// town.stl file
import stl/sen/packages/school/school.stl

package town;

class EducationCouncil 
{
    var councelor           : school.person   
    var neededTaxIncome     : f32
    var primarySchool       : school.school   
    var secondarySchool     : school.school   
}
```

- In CPP files:

```c++
  #include "stl/sen/packages/school/school.stl.h"

   class Town {
        // some class implementation ...
        
        private:
            // pointer to our school object
            school::SchoolInterface * school_
  };
```
