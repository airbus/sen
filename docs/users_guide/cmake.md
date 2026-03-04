# Using CMake

## To build a package

To build a Sen package, use the `add_sen_package` function:

```cmake
add_sen_package(
  TARGET <target>
  [MAINTAINER <maintainer>]
  [DESCRIPTION <description>]
  [VERSION <version>]
  [BASE_PATH base_path]
  [EXPORT_NAME name of the Sen package exported, defaults to target's name if not set]
  [SCHEMA_PATH path]
  [GEN_HDR_FILES variable]
  [CODEGEN_SETTINGS file]
  [HLA_OUTPUT_DIR path]
  [SOURCES [files...]]
  [DEPS [dependencies...]]
  [PRIVATE_DEPS [dependencies...]]
  [STL_FILES [files...]]
  [HLA_FOM_DIRS [dirs...]]
  [HLA_MAPPINGS_FILE [files...]]
  [EXPORTED_CLASSES name of the classes exported by the package (if null, the exported classes are parsed automatically from the source files)]
  [IS_COMPONENT flag used if we want to implement a custom Sen component]
  [NO_SCHEMA flag used when we dont want to generate a JSON schema for the YAML configuration files of the package]
  [PUBLIC_SYMBOLS flag used when we want to mark the symbols of the generated code as visible]
  [TEST_TARGET name] if provided, a (static) test target is created (it exposes the internal package symbols)
)
```

It takes the following parameters:

| Name                | Mandatory        | Multiple         | Description                                                                                                                 |
| ------------------- | ---------------- | ---------------- | --------------------------------------------------------------------------------------------------------------------------- |
| `TARGET`            | :material-check: |                  | Name of the target that will be created for this component.                                                                 |
| `MAINTAINER`        |                  |                  | Name of the maintainer/author.                                                                                              |
| `DESCRIPTION`       |                  |                  | Textual description of the implemented function.                                                                            |
| `VERSION`           |                  |                  | Semantic versioning for the generated binary.                                                                               |
| `BASE_PATH`         |                  |                  | Import path for Sen files and the one to be used by its dependencies.                                                       |
| `EXPORT_NAME`       |                  |                  | Name of the exported Sen package. Defaults to the target's name if not set.                                                 |
| `SCHEMA_PATH`       |                  |                  | Path where you want to have the data model schema.                                                                          |
| `GEN_HDR_FILES`     |                  |                  | Variable containing generated header files.                                                                                 |
| `CODEGEN_SETTINGS`  |                  |                  | Path to the JSON file containing the settings for the code generator.                                                       |
| `HLA_OUTPUT_DIR`    |                  |                  | Path where HLA output files will be placed.                                                                                 |
| `SOURCES`           |                  | :material-check: | List of source files (relative to this file).                                                                               |
| `DEPS`              |                  | :material-check: | List of libraries that the component will publicly link to.                                                                 |
| `PRIVATE_DEPS`      |                  | :material-check: | List of libraries that the component will privately link to.                                                                |
| `STL_FILES`         |                  | :material-check: | List of STL files to process from which the Sen package will generate code.                                                 |
| `HLA_FOM_DIRS`      |                  | :material-check: | Directories containing HLA FOM files.                                                                                       |
| `HLA_MAPPINGS_FILE` |                  | :material-check: | Files containing HLA mappings.                                                                                              |
| `EXPORTED_CLASSES`  |                  |                  | Name of the classes exported by the package (if null, the exported classes are parsed automatically from the source files). |
| `IS_COMPONENT`      |                  |                  | If present, a sen component is generated.                                                                                   |
| `PUBLIC_SYMBOLS`    |                  |                  | If present, all symbols of the generated code are visible.                                                                  |
| `NO_SCHEMA`         |                  |                  | If present, no schema is generated for the data model.                                                                      |
| `TEST_TARGET`       |                  |                  | If used, a (static) test target is created exposing internal package symbols.                                               |

For example,

```cmake
add_sen_package(
        TARGET
          calculators
        MAINTAINER
          "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
        VERSION
          "0.0.1"
        DESCRIPTION
          "Example package"
        SOURCES
          src/casio_calculator.cpp
          src/faulty_calculator.cpp
        STL_FILES
          stl/calculator.stl
        SCHEMA_PATH
          ${CMAKE_CURRENT_BINARY_DIR}
)
```

### How to use the `DEPS` and `PRIVATE` options for dependencies

The `DEPS` and `PRIVATE_DEPS` options of the `add_sen_package` function are used to link
dependencies to a package. These dependencies can be of two different types:

- Other Sen packages: these dependencies are linked against our package and their `BASE_PATH`
  properties are transmitted to our package, so we can include STL/XML files in the generated code
  of our package. **This is very important**, as only linking Sen packages is not enough for using
  their interface files, we need to add them to the `DEPS` option.
- Dependencies that are not Sen packages: These are just linked to our Sen package as dependencies.
  `DEPS` links the dependency as `PUBLIC` and `PRIVATE_DEPS` does it as `PRIVATE`.

#### Using Sen packages as dependencies of other Sen packages

If you plan to use the package as a dependency for other packages, and therefore want the symbols in
that package to be exported, set the `PUBLIC_SYMBOLS` flag in the `add_sen_package` function. This
can also be achieved by calling `sen_generate_package`, which is an alias of `add_sen_package` with
the `PUBLIC_SYMBOLS` flag on. However, the use of `sen_generate_package` is not advised as it will
be deprecated in the future, and it was only added to keep retro-compatibility with previous
versions.

**Important note**: only types in the generated code are exported, if you want any of the types of
your package implementation (the ones defined in the `SOURCES`) to be exported, you need to
explicitly export it using the `SEN_EXPORT` macro, as the symbols in the package will be hidden by
default.

Below is an example of two packages, where one of them is linked as a dependency to the other:

```cmake
add_sen_package(
        TARGET
          hla_fom
        BASE_PATH
          ${CMAKE_CURRENT_LIST_DIR}
        HLA_OUTPUT_DIR
          hla_fom
        HLA_FOM_DIRS
          rpr netn link16
        GEN_HDR_FILES
          fom_headers
        HLA_MAPPINGS_FILE
          base_mappings.xml
        SCHEMA_PATH
          ${CMAKE_CURRENT_BINARY_DIR}
        PUBLIC_SYMBOLS
)

add_sen_package(
        TARGET
          aircrafts
        MAINTAINER
          "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
        VERSION
          "0.0.1"
        DESCRIPTION
          "Dummy entities for testing"
        STL_FILES
          stl/dummy_aircraft.stl
        SOURCES
          src/dummy_aircraft_impl.cpp
        DEPS
          hla_fom sen::util
        SCHEMA_PATH
          ${CMAKE_CURRENT_BINARY_DIR}
)
```

As you can see, the `hla_fom` package is linked as a dependency of the `aircrafts` package,
therefore it is marked with the `PUBLIC_SYMBOLS` flag.

#### How to test the internal implementation of a package? The `TEST_TARGET` option

Sen packages have all symbols hidden by default. This means that if we want to have a test
executable for the implementation details of a package, we cannot link that executable directly
against the package and expect it to work. All the symbols in the package will not be visible to the
test executable.

In order to tackle this problem, we created the `TEST_TARGET` option on the `add_sen_package`
function. In the `TEST_TARGET` option, the user can specify the name of a test_target (e.g.
`<target_name>_test`) which will finally be a `STATIC` library containing the package code.

Then, the test executable can be linked against that test target and, due to the static linking, all
the symbols will be available in our executable.

However, adding the `TEST_TARGET` option changes how the `add_sen_package` function works: in order
to avoid compiling the package twice (one as `<target_name>` and one as `<target_name>_test`), we
use an intermediate cmake `OBJECT` target called `<target_name>_obj`, which is the one that is
actually compiled. **This implies that any modification that we want to do to our package outside
the `add_sen_package` function, we want to do it to the `<target_name>_obj` and not to the
`<target_name>`.**

Below is an example of a Sen package with the `TEST_TARGET` option enabled:

```cmake
add_sen_package(
  TARGET
    my_package
  SOURCES
    ${base_sources}
  STL_FILES
    stl/configuration.stl stl/options.stl stl/types.stl
  DEPS
    asio::asio
    llhttp::llhttp
    nlohmann_json::nlohmann_json
    spdlog::spdlog
  BASE_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}
  TEST_TARGET
    my_package_test
)

target_include_directories(my_package_obj PUBLIC directories/to/include)

add_executable(gtest_for_my_package test.cpp)

target_link_libraries(gtest_for_my_package PRIVATE my_package_test)
```

As you can see in the example above, the include directories property is set to `my_package_obj` and
not to my_package or `my_package_test`. Afterward, the `gtest_for_my_package` executable is linked
against the `my_package_test`.

#### How to implement a custom Sen component, the `IS_COMPONENT` flag (Advanced)

If you want to implement your own Sen component, just add the `IS_COMPONENT` flag to the
`add_sen_package` function. Below is a usage example of the `add_sen_package` function being used to
add a Sen component:

```cmake
add_sen_package(
        TARGET
          example_component
        MAINTAINER
          "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
        VERSION
          "0.0.1"
        DESCRIPTION
          "Executes a method call during object removal"
        SOURCES
          component.cpp
        STL_FILES
          stl/example.stl
        IS_COMPONENT
)

```

## Other utilities

### Direct code generation

```cmake
sen_generate_cpp(
    TARGET <target>
    [BASE_PATH base_path]
    [STL_FILES [files...]]
    [HLA_FOM_DIRS [directories...]]
    [HLA_MAPPINGS_FILE file]
    [GEN_HDR_FILES variable]
    [CODEGEN_SETTINGS file]
)
```

For example,

```cmake
sen_generate_cpp(
    TARGET
        terminal_lib
    STL_FILES
        components/shell/stl/remote_shell.stl
)
```

or,

```cmake
sen_generate_cpp(
    TARGET
        fom_lib
    HLA_FOM_DIRS
        rpr
        netn
    GEN_HDR_FILES
        fom_headers
    HLA_MAPPINGS_FILE
        mapping.xml
)
```

### Static analysis

The `sen_enable_static_analysis` function enables clang-tidy for the target.

```cmake
sen_enable_static_analysis(ether)
```

### UML diagrams

Sen can generate PlantUML diagrams.

```cmake
sen_generate_uml(
    [BASE_PATH base_path]
    [STL_FILES [files...]] | [HLA_FOM_DIRS [directories...]]
    [CLASSES_ONLY | TYPES_ONLY | TYPES_ONLY_NO_ENUMS]
    OUT output_file
)
```

For example:

```cmake
sen_generate_uml(
    TARGET
        fom_lib_uml
    OUT
        ${CMAKE_CURRENT_BINARY_DIR}/fom.plantuml
    HLA_FOM_DIRS
        rpr
        netn
    CLASSES_ONLY
)
```

### Target configuration

The function, `sen_configure_target(<target>)` adds all the compiler and linker flags, warnings and
options for building a Sen binary target.

### Python and YAML generation

You can use Python to store your configuration data. This provides a more flexible, readable and
powerful way of managing complex parameters.

The Sen kernel takes the configuration data from YAML files, but Python can be used to generate the
YAML files. To help you in this task, Sen provides means for generating Python code consisting in
[dataclasses](https://realpython.com/python-data-classes/) for a type-safe approach.

There is a CMake function that helps you invoke the code generator:

```cmake
sen_generate_python(
    TARGET <target>
    [BASE_PATH base_path]
    [STL_FILES [files...]]
    [HLA_FOM_DIRS [directories...]]
    [HLA_MAPPINGS_FILE file]
    [GEN_HDR_FILES variable]
)
```

It works like the other code generation functions. There will be one output .py file per input file
(STL or HLA), and the output folder structure will mimic that of the input.

Once you have your generated Python code, you will want to write the configuration Python file(s).
There is another CMake function that helps you in generating the final YAML file:

```cmake
sen_generate_yaml(
    TARGET <target>
    SCRIPT python_script
    OUTPUT output_file
    [DEPS [dependencies...]]
)
```

In the `DEPS` argument you need to set all the dependencies, and be sure to add the target that you
used to generate the Python code. This way, this function will automatically configure the
`PYTHONPATH` with the folders that host the generated code.

Keep in mind that the generated Python code and the generated YAML file are not meant to be kept
under version control. The idea is to have a single source of truth: your config script for the
data, and the STL (or HLA) files for the types.

If [mypy](https://mypy-lang.org/) is found, this function will run the checks on the script.

### Obtaining external interfaces information

Sen provides the necessary functionality to obtain interfaces files (such as `stls` or HLA FOM
`xmls`) when consuming packages from outside your project.

The following CMake utility obtains interface-related properties from a target such as `STL_FILES`,
`BASE_PATH` or `HLA_FOM_DIRS`.

```cmake

sen_generate_python(
    TARGET <targetName>
    INSTALLATION_DIR <directory>
)
```

After calling this function, you will be able to fetch the `INSTALL_STL_FILES`, `INSTALL_BASE_PATH`
and `INSTALL_HLA_FOM_DIRS` properties who will contain the necessary paths and information for you
to generate code using the functions described above based on your needs.

A more detailed guide on this function and on the whole process of exporting and consuming external
interfaces can be found in the following guides:

- [How to export interfaces in Sen-based projects](../howto_guides/exportable_interfaces.md)
- [How to consume external interfaces in Sen-based projects](../howto_guides/consuming_interfaces.md)
- [How to generate CMake packages to export pre-built interfaces](../howto_guides/generate_interface_packages.md)

### Automatic configuration and installation of `-config.cmake.in` files

A `-config.cmake.in` file contains the configuration of a CMake package. In a nutshell, this files
contain the CMake code of a project that will be executed when you call `find_package()` to consume
it externally. Sen offers a small CMake helper who eases the installation process of this type of
files. This function will come in handy when a project contains several `-config.cmake.in` files in
different subdirectories of the project.

The function takes a list of directories who contain `-config.cmake.in` files and configures the
files so that they are generated and installed in your `CMAKE_INSTALL_PREFIX` directory. It will
also install the `-config.cmake.in` files found in the current list directory. This directory will
almost certainly match the directory where your `install.cmake` file is found, since this function
should only be used when generating a CMake install package.

```cmake
 configure_exportable_packages(
    INTERFACES_CONFIG_DIRS [directory1, directory2, ...]
)
```

More detail on functionality and information on when should this function be used can be found on
the
[How to generate CMake packages to export pre-built interfaces](../howto_guides/generate_interface_packages.md)
guide.
