# Generate exportable interfaces in your Sen-based project.

In this guide, we will understand how to configure your project so that its interfaces (STL or HLA
FOM files, although we will focus on STL) can be exported. This will ensure that interfaces can
later be consumed as explained on the
[how to consume external interfaces in Sen-based projects](consuming_interfaces.md) guide.

We will cover the following steps:

- Organization of your project' directories.
- Configuration of installation rules (exporting interface files).
- CMake package configuration.

## Organization of your project directories

The first step to ensuring a correct export of your interfaces is to have a healthy organization of
your project source code directories.

This guide will start from a Sen cookie-cutter based repository, but it can be followed no matter
what your repository organization is.

Firstly, we need to identify the `stl` files that we want to export. Following Sen's standard, these
files are directly related to either a package, a library or a component.

Let's take an example from Sen: A 'school' package within the 'sen' project. We want to highlight
the fact that

1. This is part of the 'sen' project
2. It's a 'package'
3. It is called 'school'
4. We are defining STL files.

We should **organize our STL files and folders in the same way they will be imported**. We want our
exportation to match

`stl/sen/packages/school`

So the folder structure would look like:

```shell
└──  stl
    └── sen
        └── packages
            └── school
                ├── class_room.stl
                ├── person.stl
                ├── student.stl
                ├── superintendent.stl
                └── teacher.stl
```

Note that applying this directory organization will also mean changing the way of including (`cpp`)
and importing (`stl`) the `stl` files inside your project: instead of importing `person.stl` or
including `stl/person.stl.h`, you would need to add the full path described above.

______________________________________________________________________

> Can we ignore this directory organization when importing other STL files?

: Technically, yes, but it is **not** recommended. Ignoring the organization will create ambiguous
import paths when consuming the interfaces. For example, if we left the example directory as it
originally was, the consuming developer can still use the interfaces but instead of importing
`"stl/sen/packages/school/person.stl"`, he would just import `"person.stl"`. This may not only be
difficult to understand, but it can even **shadow** other files in case several consumed projects
have `stl` files with the same name.

### Configuration of our `BASE_PATH`

When generating code from `xml` or `stl` files, Sen offers the possibility of adding a `BASE_PATH`.
The `BASE_PATH` facilitates the importing of these files, allowing to set a common path for the
files that will be generated.

If we follow the directory organization set above, we **need** to set the base path when generating
our targets to the **root of the component**. This means setting the BASE_PATH so that when we
include files, the path to include is the one we added to the directory.

Let's follow the Sen school example. If we start from the root of the project, the original `stl`
files resided on `components/recorder/stl/school.stl` but now, they are at
`components/recorder/stl/sen/components/recorder/school.stl`. This means that if we want our
importing path to be `import stl/sen/components/recorder/school.stl`, we need to set our base path
to `components/recorder`.

______________________________________________________________________

> What happens if the `BASE_PATH` argument is ignored or set to a different value?

: Using a different `BASE_PATH` would make your project way more vulnerable to issues when both
exporting and consuming its interfaces. Sen code generator expects the files to be organized in a
certain way. Not adding the `BASE_PATH` can lead to issues such as import and include errors when
generating code from the interfaces, both in this project and in the project that will consume the
interfaces. Using a different `BASE_PATH` will require you to adapt not only your CMake, code and
repository structure, but also the one of the developer that consumes your project as a package.
Promoving a standard way of operating is the best way to ensure that compatibility is maintained
across the large number of developers that will export and consume Sen-based packages.

## Configuration of installation rules (exporting `stl` files)

After having reorganized the directory and adapted your project to this reorganization, we need to
configure the adequate CMake install rules to add our `stl` files to our exported package.

If you have followed
[Sen's guide on how to export a Sen-based project as a Conan package](creating_conan_packages.md),
you may already be familiar with CMake install rules and the importance they have when exporting
source code inside our CMake packages. To export our interfaces, we need to add similar rules that
indicate CMake where will our `stl` files be placed once we generate an exportable package.

The standard location where our interfaces will be installed inside a CMake package is the directory
`interfaces`. Using this directory will allow us to both have a clear view of where are our
interfaces located, and also facilitate the importing mechanism.

Following our **school** example, we would need to add installation rules for all of our files
inside the `stl` directory. This can be done by editing `cmake/targets/components/school.cmake` (or
any other location where the CMake code related to the school component is) and adding the following
code:

```cmake
set(school_source_dir ${CMAKE_SOURCE_DIR}/components/school)
set(_school_stl_files ${school_source_dir}/stl/sen/packages/school/class_room.stl
        ${school_source_dir}/stl/sen/packages/school/person.stl
        ${school_source_dir}/stl/sen/packages/school/student.stl
        ${school_source_dir}/stl/sen/packages/school/superintendent.stl
        ${school_source_dir}/stl/sen/packages/school/teacher.stl
)

install(FILES ${_school_stl_files} DESTINATION interfaces/stl/sen/packages/school)
```

Note that the keyword **interfaces** that we mentioned before precedes the configured path in the
[previous section](exportable_interfaces.md#organization-of-your-project-directories). The path
inputted right after `interfaces` needs to be **the path to the `stl` files from the component's
root directory.**

This CMake code will install all the school's `stl` files inside the adequate install directory. You
will need to configure as many `install` rules as you need, depending on your directory organization
and the number of `stl` files you need to export.

## CMake package configuration

The last step of the process involves adjusting the CMake package configuration so that our
interfaces are visible and obtainable to the consumers of the package.

Configuring a CMake package will allow us to export our project in any possible way. With a CMake
project you can use every mechanism you can think of to export your package (Conan, Nexus uploading,
ZIP files, etc.). As long as the path to the CMake files of your package is added in the
`CMAKE_PREFIX_PATH`, you will be able to use its entire functionality.

The generation of a CMake package is covered in
[Sen's guide on how to export a Sen-based project as a Conan package](creating_conan_packages.md).
After following these steps, we only need to apply a slight modification to the project's
**`-config.cmake.in`** file.

### Modifying the `-config.cmake.in` file

The whole purpose of this guide is to prepare a Sen-based project so that its interfaces can be
consumed from any external project that uses it as a dependency. This means that the external
project will need to know **where our package is installed**, so that it can get the paths and any
additional information needed to be able to use those interfaces when consumed externally.

Indicating the installation directory is as simple as adding this line to your `-config.cmake.in`
file:

```cmake
set(*your_project_name*_INSTALL_DIR ${CMAKE_CURRENT_LIST_DIR}/../../)
```

______________________________________________________________________

> What does this variable do?

: The `INSTALL_DIR` variable will simply point to the root of the directory where your package is
installed at the time of consuming it. The `../..` is added since in our standard CMake package
generation, the CMakes of the project are located inside the `cmake/project_name` directory, hence
the root directory will be located two directories behind.

______________________________________________________________________

> Why is this needed?

: The `INSTALL_DIR` variable will be available to every CMake-based project that consumes your
exported package. The correct setting of this variable allows the external project to know where
your project is, and is required in the [consuming interfaces](consuming_interfaces.md) process.
