# Generate exportable interfaces in your Sen-based project

In this guide, we will understand how to configure your project so that its interfaces (STL or HLA
FOM files, although we will focus on STL) can be exported. This will ensure that interfaces can
later be consumed as explained on the
[how to consume external interfaces in Sen-based projects](consuming_interfaces.md) guide.

"Export" is used for three unrelated things in Sen, so this page is probably not the one you want
unless your question is about shipping interface files. `SEN_EXPORT_CLASS` registers a class with
the kernel, and [Creating your first package](../getting_started/first_package.md) covers it. The
`sen generate cpp exports` command generates an exports file for symbol visibility, and
[Command line tools](../users_guide/command_line.md) documents it. This page is about installing
`.stl` files so another CMake project can build against them.

We will cover the following steps:

- Organization of your project directories.
- Configuration of installation rules (exporting interface files).
- CMake package configuration.

## Organization of your project directories

The first step to ensuring a correct export of your interfaces is to have a healthy organization of
your project source code directories.

This guide will start from a Sen cookie-cutter based repository, but it can be followed no matter
what your repository organization is.

Firstly, we need to identify the `stl` files that we want to export. Following Sen's standard, these
files are directly related to either a package, a library or a component.

Everything else follows from this:

> **The path from `BASE_PATH` to an STL file is the path everyone imports it by**, and the path of
> the generated header they include.

So the folders you choose under `BASE_PATH` are not an internal detail. They are baked into every
`import` statement and every `#include` in every project that consumes your interfaces, including
your own. Pick them so they read well and cannot collide with somebody else's.

A common shape is to put your organization or project first, then the thing the interfaces belong
to. Say the `acme` organization has a `radar` package:

```shell
packages/radar
├── src
└── stl
    └── acme
        └── radar
            ├── sar_radar.stl
            └── waveform.stl
```

With `BASE_PATH` set to `packages/radar/stl`, the import becomes:

```rust
import "acme/radar/sar_radar.stl"
```

and the generated header is included as `"acme/radar/sar_radar.stl.h"`. The intermediate folders are
entirely your choice; the generator does not require any particular depth or naming.

Note that applying this directory organization will also mean changing the way of including (`cpp`)
and importing (`stl`) the `stl` files inside your project: instead of importing `person.stl` or
including `stl/person.stl.h`, you would need to add the full path described above.

---

> Can we ignore this directory organization when importing other STL files?

: Technically, yes, but it is **not** recommended. Ignoring the organization creates ambiguous
import paths when consuming the interfaces. If the files sat directly under `BASE_PATH`, a consumer
would import `"sar_radar.stl"` rather than `"acme/radar/sar_radar.stl"`. That is harder to read, and
it can **shadow** files: two consumed projects each shipping a `sar_radar.stl` would collide, and
nothing would tell you which one you got.

### Configuration of your `BASE_PATH`

When generating code from `xml` or `stl` files, Sen offers the possibility of adding a `BASE_PATH`.
The `BASE_PATH` parameter defines the root of the directory hierarchy so that generated files
maintain common inclusion paths.

**Important:** Its behavior differs significantly depending on the file type:

- **For STL files:** The `BASE_PATH` is strictly respected. It determines the relative path used in
  code inclusions.
- **For HLA FOM files (.xml):** The `BASE_PATH` is **ignored** by the code generator. Sen enforces a
strict directory layout for HLA and always uses the **immediate parent directory** of the XML file
to build the inclusion paths.

#### STL example

To ensure correct resolution, the `BASE_PATH` should be set to the **root of the component**.

Continuing the example: the file sits at `packages/radar/stl/acme/radar/sar_radar.stl`, and we want
`import "acme/radar/sar_radar.stl"`. Everything up to and including `stl/` has to be stripped, so
`BASE_PATH` is `packages/radar/stl`.

Set it one level higher, at the package root, and the `stl/` segment stays in the path, so the
import becomes `"stl/acme/radar/sar_radar.stl"`. Sen's own components are laid out that way. Neither
is more correct.

Writing it relative to CMake, as `BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/stl`, keeps it working if
the package moves within your repository.

#### HLA FOM example

For FOM files, the inclusion path is derived automatically from the physical location of the XML
file, regardless of any `BASE_PATH` provided in CMake. Sen expects a `Grandparent/Parent/File.xml`
structure.

**Example:**

- Physical path: `fom/rpr/RPR-Base.xml`
- Sen detects `rpr` as the immediate parent.
- Any file importing/including this FOM will use the path: `"rpr/RPR-Base.xml.h"`

To ensure a consistent structure, organize your FOM directories so that the immediate parent
directory matches your intended include prefix (e.g., `rpr/`, `netn/`).

---

> What happens if the `BASE_PATH` argument is ignored or set to a different value for STL files?

: Using a different `BASE_PATH` for STL files makes your project much more vulnerable to issues when
both exporting and consuming its interfaces. Sen code generator expects the files to be organized in
a certain way. Not adding the `BASE_PATH` can lead to issues such as import and include errors when
generating code from the interfaces, both in the current project and in any project that consumes
the interfaces. Using an incorrect `BASE_PATH` will require you to adapt not only your CMake, code,
and repository structure, but also the one of the developer that consumes your project as a package.
Promoving a standard way of operating is the best way to ensure that compatibility is maintained
across the large number of developers that will export and consume Sen-based packages.

## Configuration of installation rules (exporting `stl` files)

After having reorganized the directory and adapted your project to this reorganization, we need to
configure the adequate CMake install rules to add your `stl` files to your exported package.

If you have followed
[Sen's guide on how to export a Sen-based project as a Conan package](creating_conan_packages.md),
you may already be familiar with CMake install rules and the importance they have when exporting
source code inside your CMake packages. To export your interfaces, you need to add similar rules
that tell CMake where your `stl` files go once you generate an exportable package.

The standard location where your interfaces will be installed inside a CMake package is the
directory `interfaces`. Using it gives you a clear view of where your interfaces are, and makes the
importing mechanism easier.

List the files once, so the same variable feeds both `add_sen_package` and the install rule and the
two cannot fall out of step:

```cmake
set(_radar_stl_files
    stl/acme/radar/sar_radar.stl
    stl/acme/radar/waveform.stl
)

add_sen_package(
  TARGET    radar
  BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/stl
  STL_FILES ${_radar_stl_files}
  # ... the rest of your package definition
)

install(FILES ${_radar_stl_files} DESTINATION interfaces/acme/radar)
```

The destination is `interfaces/` followed by **the same path the consumer imports**:
`acme/radar` here, because that is what `BASE_PATH` left. If the two disagree, the package installs
successfully and consumers cannot resolve the import.

Add as many `install` rules as your layout needs. One is enough when all the STL files sit in a
single directory, as they do here.

## CMake package configuration

The last step of the process involves adjusting the CMake package configuration so that your
interfaces are visible and obtainable to the consumers of the package.

Configuring a CMake package lets you export your project in any way you like. With a CMake
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
project will need to know **where your package is installed**, so that it can get the paths and any
additional information needed to be able to use those interfaces when consumed externally.

Indicating the installation directory is as simple as adding this line to your `-config.cmake.in`
file:

```cmake
set(*your_project_name*_INSTALL_DIR ${CMAKE_CURRENT_LIST_DIR}/../../)
```

---

> What does this variable do?

: The `INSTALL_DIR` variable will simply point to the root of the directory where your package is
installed at the time of consuming it. The `../..` is added since in the standard CMake package
generation, the CMakes of the project are located inside the `cmake/project_name` directory, hence
the root directory will be located two directories behind.

---

> Why is this needed?

: The `INSTALL_DIR` variable will be available to every CMake-based project that consumes your
exported package. The correct setting of this variable allows the external project to know where
your project is, and is required in the [consuming interfaces](consuming_interfaces.md) process.
