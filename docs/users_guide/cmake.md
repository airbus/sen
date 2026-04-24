# Using CMake

## To build a package

To build a Sen package, use the `add_sen_package` function:

```cmake
add_sen_package(
  TARGET <name>
  [MAINTAINER <name>]
  [DESCRIPTION <text>]
  [VERSION <version>]
  [BASE_PATH <path>]
  [EXPORT_NAME <name>]
  [SCHEMA_PATH <path>]
  [GEN_HDR_FILES <variable>]
  [CODEGEN_SETTINGS <file>]
  [HLA_OUTPUT_DIR <path>]
  [SOURCES <files...>]
  [DEPS <targets...>]
  [PRIVATE_DEPS <targets...>]
  [STL_FILES <files...>]
  [HLA_FOM_DIRS <dirs...>]
  [HLA_MAPPINGS_FILE <files...>]
  [EXPORTED_CLASSES <names...>]
  [IS_COMPONENT]
  [NO_SCHEMA]
  [PUBLIC_SYMBOLS]
  [TEST_TARGET <name>]
)
```

It takes the following parameters:

| Name                | Mandatory        | Multiple         | Description                                                                                                                                                           |
| ------------------- | ---------------- | ---------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `TARGET`            | :material-check: |                  | Name of the CMake target to create.                                                                                                                                   |
| `MAINTAINER`        |                  |                  | Person or team responsible for this package. Defaults to `"unknown"`. Note: `AUTHOR` is a deprecated alias.                                                          |
| `DESCRIPTION`       |                  |                  | Human-readable description embedded in the package metadata.                                                                                                          |
| `VERSION`           |                  |                  | Package version string. Defaults to the CMake project version.                                                                                                        |
| `BASE_PATH`         |                  |                  | Root directory used to compute relative paths for generated files and interface resolution. Defaults to `CMAKE_CURRENT_SOURCE_DIR`.                                    |
| `EXPORT_NAME`       |                  |                  | Name under which the package is registered with the Sen runtime. Defaults to `TARGET`.                                                                                |
| `SCHEMA_PATH`       |                  |                  | Directory where the JSON schema describing the package's YAML configuration will be written. Defaults to `${PROJECT_SOURCE_DIR}/schemas`.                             |
| `GEN_HDR_FILES`     |                  |                  | **Output variable.** Receives the list of header files produced by code generation, useful for adding them to IDE source groups.                                      |
| `CODEGEN_SETTINGS`  |                  |                  | Path to a settings file forwarded to the code generator (`cli_gen`).                                                                                                  |
| `HLA_OUTPUT_DIR`    |                  |                  | Subdirectory within the generated output directory for HLA-derived files. Only relevant when `HLA_FOM_DIRS` is used.                                                  |
| `SOURCES`           |                  | :material-check: | C++ source files implementing the package logic.                                                                                                                      |
| `DEPS`              |                  | :material-check: | Public dependencies. Linked with `PUBLIC` linkage; their exported Sen types are re-exported by this package.                                                          |
| `PRIVATE_DEPS`      |                  | :material-check: | Private dependencies. Linked with `PRIVATE` linkage; not visible to consumers.                                                                                        |
| `STL_FILES`         |                  | :material-check: | Sen Type Language (`.stl`) files from which C++ code is generated. Mutually exclusive with `HLA_FOM_DIRS`.                                                            |
| `HLA_FOM_DIRS`      |                  | :material-check: | Directories containing HLA FOM XML files from which C++ code is generated. Mutually exclusive with `STL_FILES`.                                                       |
| `HLA_MAPPINGS_FILE` |                  | :material-check: | HLA mapping files that customise FOM-to-C++ translation. Requires `HLA_FOM_DIRS`.                                                                                    |
| `EXPORTED_CLASSES`  |                  | :material-check: | Explicit list of class names to register with the Sen runtime. If omitted, derived automatically by scanning `SOURCES` for `SEN_EXPORT_CLASS()` macros.               |
| `IS_COMPONENT`      |                  |                  | Marks this package as a Sen component. Enables component-specific code generation (e.g. embedding the component name in the schema).                                  |
| `NO_SCHEMA`         |                  |                  | Skip JSON schema generation. Use this when the package has no YAML configuration interface or when schema generation is handled elsewhere.                             |
| `PUBLIC_SYMBOLS`    |                  |                  | Export generated code symbols with public visibility. Required when the generated headers are consumed by Python bindings or other shared libraries.                   |
| `TEST_TARGET`       |                  |                  | If set, also creates a `STATIC` library with this name exposing internal symbols, making them accessible from unit tests without needing a shared-library boundary.   |

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

`sen_generate_cpp` generates C++ code from STL or HLA FOM files and adds the result to an existing
CMake target. For the common case of building a full Sen package, prefer `add_sen_package()` instead.

```cmake
sen_generate_cpp(
    TARGET <name>
    [OUTPUT_DIR <path>]
    [BASE_PATH <path>]
    [CODEGEN_SETTINGS <file>]
    [GEN_HDR_FILES <variable>]
    [SCHEMA_FILE <path>]
    [SCHEMA_COMPONENT_NAME <name>]
    [HLA_OUTPUT_DIR <path>]
    [STL_FILES <files...>]
    [HLA_FOM_DIRS <dirs...>]
    [HLA_MAPPINGS_FILE <files...>]
    [VISIBLE_CLASSES <YES|NO>]
)
```

| Name                   | Mandatory        | Multiple         | Description                                                                                                                         |
| ---------------------- | ---------------- | ---------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `TARGET`               | :material-check: |                  | An already-created CMake target to which generated sources will be added.                                                           |
| `OUTPUT_DIR`           |                  |                  | Directory where generated files are written. Defaults to `${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_generated`.                         |
| `BASE_PATH`            |                  |                  | Root directory for import resolution and relative-path computation. Defaults to `CMAKE_CURRENT_SOURCE_DIR`.                         |
| `CODEGEN_SETTINGS`     |                  |                  | Path to a settings file forwarded to the code generator (`cli_gen`).                                                                |
| `GEN_HDR_FILES`        |                  |                  | **Output variable.** Receives the list of generated header files, useful for adding them to IDE source groups.                      |
| `SCHEMA_FILE`          |                  |                  | If set, triggers JSON schema generation and writes the schema to this path.                                                         |
| `SCHEMA_COMPONENT_NAME`|                  |                  | When set alongside `SCHEMA_FILE`, the schema is generated as a component schema with this name embedded.                            |
| `HLA_OUTPUT_DIR`       |                  |                  | Subdirectory within `OUTPUT_DIR` for HLA-derived files. Only relevant when `HLA_FOM_DIRS` is used.                                  |
| `STL_FILES`            |                  | :material-check: | Sen Type Language (`.stl`) files from which C++ code is generated. Mutually exclusive with `HLA_FOM_DIRS`.                         |
| `HLA_FOM_DIRS`         |                  | :material-check: | Directories containing HLA FOM XML files from which C++ code is generated. Mutually exclusive with `STL_FILES`.                    |
| `HLA_MAPPINGS_FILE`    |                  | :material-check: | HLA mapping files that customise FOM-to-C++ translation. Requires `HLA_FOM_DIRS`.                                                  |
| `VISIBLE_CLASSES`      |                  |                  | Whether generated class symbols are exported with public visibility (`YES` or `NO`). Required when consumed across shared-library boundaries. |

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

Sen can generate PlantUML diagrams from STL or HLA FOM files.

```cmake
sen_generate_uml(
    TARGET <name>
    OUT <file>
    [BASE_PATH <path>]
    [STL_FILES <files...>]
    [HLA_FOM_DIRS <dirs...>]
    [HLA_MAPPINGS_FILE <files...>]
    [CLASSES_ONLY | TYPES_ONLY | TYPES_ONLY_NO_ENUMS]
)
```

| Name                | Mandatory        | Multiple         | Description                                                                                                                      |
| ------------------- | ---------------- | ---------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `TARGET`            | :material-check: |                  | Name of the custom CMake target that triggers diagram generation.                                                                |
| `OUT`               | :material-check: |                  | Path to the output `.puml` file to be written.                                                                                   |
| `BASE_PATH`         |                  |                  | Root directory for import resolution. Defaults to `CMAKE_CURRENT_SOURCE_DIR`.                                                    |
| `STL_FILES`         |                  | :material-check: | Sen Type Language (`.stl`) files to diagram. Mutually exclusive with `HLA_FOM_DIRS`.                                            |
| `HLA_FOM_DIRS`      |                  | :material-check: | Directories containing HLA FOM XML files to diagram. Mutually exclusive with `STL_FILES`.                                       |
| `HLA_MAPPINGS_FILE` |                  | :material-check: | HLA mapping files forwarded to the diagram generator. Requires `HLA_FOM_DIRS`.                                                  |
| `CLASSES_ONLY`      |                  |                  | Include only class and interface definitions. Omits primitive types, structs, and enumerations.                                  |
| `TYPES_ONLY`        |                  |                  | Include only type definitions (structs, enums, aliases). Omits class and interface diagrams.                                     |
| `TYPES_ONLY_NO_ENUMS`|                 |                  | Like `TYPES_ONLY` but also omits individual enumerator values, showing only the enum names.                                     |

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
    TARGET <name>
    SCRIPT <file>
    OUTPUT <file>
    [DEPS <targets...>]
)
```

| Name     | Mandatory        | Multiple         | Description                                                                                                                                                      |
| -------- | ---------------- | ---------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `TARGET` | :material-check: |                  | Name of the custom CMake target that triggers YAML generation.                                                                                                   |
| `SCRIPT` | :material-check: |                  | Path to the Python script that produces the YAML. Called as `python script.py <output_file>`.                                                                    |
| `OUTPUT` | :material-check: |                  | Path where the generated YAML file will be written.                                                                                                              |
| `DEPS`   |                  | :material-check: | Targets this script depends on. Python-language targets (created with `sen_generate_python`) have their generated directory automatically prepended to `PYTHONPATH`. |

In the `DEPS` argument you need to set all the dependencies, and be sure to add the target that you
used to generate the Python code. This way, this function will automatically configure the
`PYTHONPATH` with the folders that host the generated code.

If [mypy](https://mypy-lang.org/) is found, this function also creates a `<TARGET>_check` target
that type-checks the script, and makes `TARGET` depend on it so type checking runs before YAML
generation.

Keep in mind that the generated Python code and the generated YAML file are not meant to be kept
under version control. The idea is to have a single source of truth: your config script for the
data, and the STL (or HLA) files for the types.

### Obtaining external interfaces information

Sen provides the necessary functionality to obtain interfaces files (such as `stls` or HLA FOM
`xmls`) when consuming packages from outside your project.

The following CMake utility obtains interface-related properties from a target such as `STL_FILES`,
`BASE_PATH` or `HLA_FOM_DIRS`.

```cmake
get_external_interfaces(
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
