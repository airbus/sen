# Create and use CMake packages for interfaces

After covering how to both prepare your project to export interfaces in the
[how to export interfaces in Sen-based projects](exportable_interfaces.md) guide and learning to
consume them externally in the
[how to consume external interfaces in Sen-based projects'](consuming_interfaces.md) guide, we will
now learn how to create CMake packages where specific interfaces are exported as straightly
consumable code.

With the detailed guide on how to consume interfaces, you are able to use the source files (`stl`)
of the exported interfaces and generate any code of your likings (several languages, different
combination of files, etc.). The purpose of this guide is to facilitate the consumption of certain
interfaces which are almost always exported and consumed in the same way.

## Creating CMake packages: `-config.cmake.in` files

As we have seen in previous guides, the `-config.cmake.in` file is a CMake file that will be
automatically included inside your project when calling CMake's `find_package()`. To export our
interfaces, we need to configure one of these files with the code we want to execute when the file
is included.

### Setting up the file

The first lines of the `-config.cmake.in` file are typically shared. They define the package
configuration and set some variables that CMake needs for internal behavior.

```cmake
@PACKAGE_INIT@

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}")
```

### Referencing the main project

When exporting interfaces of a project, we need to ensure that the project itself is referenced in
the `-config.cmake.in` files of the interfaces. The best way to do so is to apply a block of code
where we attempt to find the original package only if it has not been included yet:

```cmake
if (NOT __dep_sen)
    set(__dep_sen YES)
    find_package(sen CONFIG REQUIRED)
endif ()
```

### Finding relevant information about the interfaces

After ensuring that the parent project has been included, we need to use the described mechanism in
the [how to consume external interfaces in Sen-based projects'](consuming_interfaces.md) guide to
obtain relevant information of our interfaces such as `stl` files, the `BASE_PATH` or the `fom`
include directories.

In this example, we are generating the interfaces of the `sen::replayer` component, so we need to
obtain its `BASE_PATH` and `stl` files.

```cmake
get_external_interfaces(TARGET sen::replayer INSTALLATION_DIR ${SEN_INSTALL_DIR})

get_target_property(_sen_replayer_stl_files sen::replayer INSTALL_STL_FILES)
get_target_property(_sen_replayer_base_path sen::replayer INSTALL_BASE_PATH)
```

Note that we can both call the `get_external_interfaces` function and use the `${SEN_INSTALL_DIR}`
as we have included Sen in the previous step.

### Generating interface code.

After obtaining the relevant information of your interfaces, it's time to configure the function
that will generate the interfaces' code when the package is consumed. The function written here will
be **unchangeable** upon consumption, so the purpose of this guide is to generate the interfaces in
the way they will always be consumed. If the user wants to consume the interfaces in a different way
(e.g. in `python` instead of `cpp`, using a `Sen` package instead of a component, etc.), then the
steps of the [how to consume external interfaces in Sen-based projects'](consuming_interfaces.md)
guide should be followed instead.

In our example case, we would write a Sen package where we build the interfaces for the
`sen::replayer` component.

```cmake
add_sen_package(
        TARGET sen_replayer_interfaces
        BASE_PATH ${_sen_replayer_base_path}
        STL_FILES ${_sen_replayer_stl_files}
        DEPS sen::db
)
```

Note that any dependency the original package/component/library had **must** be included in the
interface package generation. If this package is not configured correctly, you **won't** see any
error in your compilation, the error will appear when other developers try to consume your package.

## Include our -cmake.config.in files in the project's installation.

After writing enough `-cmake.config.in` files, you need to configure your project's `install.cmake`
file to ensure that the `-cmake.config.in` files are installed and generated correctly when you call
`cmake install`.

Sen brings a CMake function named `configure_exportable_packages`. This function receives an
`INTERFACES_CONFIG_DIRS` argument, which takes a list of directories containing `-cmake.config.in`
files. The helper takes care of generating and installing the code for all the `-cmake.config.in`
files present in the given directories and also in the current listed directory (directory where the
`install.cmake` file is located).

Using the helper is as simple as including this line in your `cmake.install` file, specifying any
additional directory that contains interfaces in the `INTERFACES_CONFIG_DIRS` argument.

```cmake
configure_exportable_packages(INTERFACES_CONFIG_DIRS ${CMAKE_CURRENT_LIST_DIR}/interfaces)
```

Note that the function will also install your project's `-config.cmake.in` file (e.g.,
`sen-config.cmake.in`, located in Sen's `cmake/util` directory), so any content referring this
particular `-config.cmake.in` file in your `install.cmake` can be deleted.

## Considerations

The `-config.cmake.in` file is only executed when consuming the CMake package via `find_package()`.
This means that there is no straightforward way of ensuring that what you write here is correct,
other than testing the consumption locally by installing your project and test it externally
(calling `find_package()` from another project). This error may not affect you, but the person who
consumes your package, so ensuring that the package can be consumed appropriately is a great
practice before uploading your CMake interfaces packages.

## Consuming CMake packages for interfaces

If any project you depend on has generated a CMake package for a desired interface, you will need
to:

1. Ensure that the project containing the interface's CMake package is correctly added to your
   `CMAKE_PREFIX_PATH`. This depends on the package-consumption method that you follow (e.g.,
   `conan` does this automatically, but with a raw installation from a ZIP file you will need to set
   the `CMAKE_PREFIX_PATH` manually).

2. Use `find_package()` with the name of the package to find it. The package name will correspond to
   the filename that precedes the `-config.cmake.in` extension.

3. Use the declared target names inside the `-config.cmake.in` to use the imported interfaces. These
   names can be set to anything the developer who exports the package wants, so we strongly
   recommend documenting the target and package names accordingly on each project's documentation.
   If no documentation is available, you can always consult the `-config.cmake.in` file on the
   project's repository.
