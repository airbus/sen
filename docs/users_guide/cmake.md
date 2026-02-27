# Using CMake

## To build a package

Use the `add_sen_package` function.

```cmake
add_sen_package(
    TARGET <target>
    MAINTAINER <maintainer>
    [DESCRIPTION <description>]
    [VERSION <version>]
    SOURCES [files...]
    [DEPS [dependencies...]]
    [BASE_PATH base_path]
    [STL_FILES [files...]]
    [PLANTUML output]
    [CODEGEN_SETTINGS file]
    [ALSO_STATIC value]
    [EXPORT_NAME name]
    [SCHEMA_PATH path]
    [NO_SCHEMA]
    [COMPONENT_SCHEMA]
)
```

It takes the following parameters:

| Name               | Mandatory        | Multiple         | Description                                                                                |
| ------------------ | ---------------- | ---------------- | ------------------------------------------------------------------------------------------ |
| `TARGET`           | :material-check: |                  | Name of the target that will be created for this component.                                |
| `MAINTAINER`       |                  |                  | Name of the maintainer/author.                                                             |
| `VERSION`          |                  |                  | Semantic versioning for the generated binary.                                              |
| `DESCRIPTION`      |                  |                  | Textual description of the implemented function.                                           |
| `SOURCES`          | :material-check: | :material-check: | List of source files (relative to this file).                                              |
| `BASE_PATH`        |                  | :material-check: | Import path for sen files and the one to be used by its dependencies.                      |
| `STL_FILES`        |                  | :material-check: | List of STL files to process with the component.                                           |
| `DEPS`             |                  | :material-check: | List of libraries that the component will privately link to.                               |
| `PLANTUML`         |                  |                  | Name of the output plantuml diagram to generate.                                           |
| `CODEGEN_SETTINGS` |                  |                  | Path to the JSON file containing the settings for the code generator.                      |
| `ALSO_STATIC`      |                  |                  | True if you also want to generate a static library.                                        |
| `EXPORT_NAME`      |                  |                  | Name of the exported Sen package. Defaults to the target's name.                           |
| `SCHEMA_PATH`      |                  |                  | Path where you want to have the data model schema.                                         |
| `NO_SCHEMA`        |                  |                  | Present if you don't want to generate any schema for the data model.                       |
| `COMPONENT_SCHEMA` |                  |                  | Tells Sen to treat this schema as a "component" (this is normally used internally by Sen). |

For example,

```cmake
add_sen_package(
    TARGET
        basic_package
    MAINTAINER
        "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
    VERSION
        "0.0.1"
    DESCRIPTION
        "Example package skeleton"
    SOURCES
        lang/my_class.h
        lang/my_class.cpp
    STL_FILES
        stl/basic_package/my_class.stl
        stl/basic_package/basic_types.stl
)
```

## To build a component

Use the `add_sen_component` function.

```cmake
add_sen_component(
    TARGET <target>
    MAINTAINER <maintainer>
    [DESCRIPTION <description>]
    [VERSION <version>]
    SOURCES [files...]
    [DEPS [dependencies...]]
    [STL_FILES [files...]]
    [SCHEMA_PATH path]
)
```

It takes the following parameters:

| Name          | Mandatory        | Multiple         | Description                                                  |
| ------------- | ---------------- | ---------------- | ------------------------------------------------------------ |
| `TARGET`      | :material-check: |                  | Name of the target that will be created for this component.  |
| `MAINTAINER`  |                  |                  | Name of the maintainer/author.                               |
| `VERSION`     |                  |                  | Semantic versioning for the generated binary.                |
| `DESCRIPTION` |                  |                  | Textual description of the implemented function.             |
| `SOURCES`     | :material-check: | :material-check: | List of source files (relative to this file).                |
| `STL_FILES`   |                  | :material-check: | List of STL files to process with the component.             |
| `DEPS`        |                  | :material-check: | List of libraries that the component will privately link to. |
| `SCHEMA_PATH` |                  |                  | Path where you want to have the data model schema.           |

For example,

```cmake
add_sen_component(
    TARGET
        ether
    MAINTAINER
        "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
    VERSION
        "0.0.1"
    DESCRIPTION
        "Ethernet transport for sen"
    SOURCES
        components/ether/lang/component.cpp
    STL_FILES
        components/ether/stl/configuration.stl
)
```

The `DEPS` option is just a helper. For example, id we would need to link to `asio` and `spdlog`, it
would be equivalent to:

```cmake
target_link_libraries(
        ether
        PRIVATE $<BUILD_INTERFACE:asio>
        PRIVATE $<BUILD_INTERFACE:spdlog::spdlog>
)
```

### To generate a package out of interface definition files

```cmake
sen_generate_package(
    TARGET <target>
    [BASE_PATH base_path]
    [STL_FILES [files...]]
    [HLA_FOM_DIRS [directories...]]
    [HLA_MAPPINGS_FILE file]
    [HLA_OUTPUT_DIR path]
    [GEN_HDR_FILES variable]
    [CODEGEN_SETTINGS file]
)
```

For example,

```cmake
sen_generate_package(
    TARGET
        fom
    HLA_FOM_DIRS
        rpr
        netn
    GEN_HDR_FILES
        fom_headers
    HLA_MAPPINGS_FILE
        mapping.xml
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

The `sen_enable_static_analysis` function enables clang-tidy.

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
