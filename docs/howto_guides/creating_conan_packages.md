# Creating your own Conan package

## Conanfile

The Conan configuration of a project resides in the conanfile.py. This file should be placed in the
root of the repository. It contains the necessary configuration, data and functions to make Conan
fetch dependencies and create your package.

## Packages and libraries

As previously mentioned, in Sen you write packages. Packages can be loaded to create objects and be
run on components, so they are perfect candidates for Conan. To consume a Sen package, you will only
need the dynamic libraries (.so in Linux, .dll in Windows).

You might also want to export and import libraries or executables as Conan packages.

Exporting packages and libraries to a Conan package requires configuring the installation of the
desired binaries and headers using CMake's `install()` function.

Normally, Sen-based projects contain a dedicated CMake directory where we have a dedicated file for
every major target, and classify them in folders based on their type. For example:

```plaintext
cmake
├── docker
│   └── ...
├── targets
│   ├── apps
│   │   ├── ...
│   ├── packages
│   │   ├── ...
│   ├── docs
│   │   ├── ...
│   ├── libs
│   │   ├── ...
│   └── test
│       ├── ...
├── test
│   ├── ...
├── tps
│   ├── ...
└── util
    ├── install.cmake
    ├── myproject-config.cmake.in
    └── ...
```

In this CMake dir, notice that there are two shown files under the `util` directory: `install.cmake`
and `myproject-config.cmake.in`.

The `myproject-config.cmake.in` file ensures that libraries and packages exported through Conan can
be located when using CMake's `find_package()` command and are visible to consumers.

```cmake
@PACKAGE_INIT@

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}")
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/myproject_targets.cmake")
```

The `install.cmake` file should be pretty similar in any project.

Reading through this file, you will find different sections, separated by dashed comments.

- In the targets section, you will need to firstly add any target (library, app, component or
  package) that you want to export.
- In the configuration of install directories section, you will need to say where you want the
  CMake files to be installed. Typically, this will be `cmake/your_project_name`. Any additional
  directory can also be added in this section (Sen adds examples here).
- The export targets section defines the file where the targets will be exported. This file is
  critical, since it will be the one read by any external CMake that consumes your project as a
  Conan package. Ensure to comply with naming and add the namespace defined at the beginning of the
  install.cmake file.
- The configuration file for CMake-based user consumption section will generate CMake's package
  configuration based on the contents of `myproject-config.cmake.in`. It generates versioning
  information in `myprojectConfigVersion.cmake`, along with the `config.cmake` file.
- Finally, you need to configure the project name in the Register package section.

## Configuring libraries in CMake files

Exporting libraries requires you to include their headers (`.h` and `.inl`), so you need to add the
`install` calls for them. The path specified in the third argument of the command is the directory
where the files will be copied inside the Conan package. Ensure that these paths are inside the
`include_directories` defined `INSTALL` directory.

```cmake
install(FILES ${core_headers} DESTINATION libs/core/include)
```

In general, use the `install()` function to add anything that needs to be present in the exported
package.

Use `target_include_directories()` to define the `BUILD` and `INSTALL` folders where the headers
will be found. The `BUILD` interface should use `PROJECT_SOURCE_DIR` as its reference, while the
`INSTALL` interface is relative to the install root of the project.

### Exporting auto-generated headers

In Sen-based libraries, code can be auto-generated out of STLs or HLA FOMs. There are some
applications where the Conan package should contain the **already compiled** files from STL, not the
STL files themselves. This can come in handy when the developer consuming the Conan package may not
need to generate code. When generating C++ code from STL files using Sen CMake macros such as
`sen_generate_cpp()` or `sen_generate_package()`, we can use the `GEN_HDR_FILES` argument to specify
a variable with the list of C++ generated files. This list will contain the path to each file, so we
can use it in the `install()` macro to copy the output files to your install directory, which will
be later packed inside your Conan package. For example,

```cmake
sen_generate_cpp(
  TARGET kernel
  STL_FILES ${stl_files}
  BASE_PATH ${CMAKE_SOURCE_DIR}/libs/kernel
  GEN_HDR_FILES stl_output_files
)

install(FILES ${stl_output_files} DESTINATION libs/kernel/stl/include/kernel)
```

What we have configured until now is CMake's `install` routine. In a nutshell, CMake installs build
artifacts, including executables, libraries, and configuration files, to specified directories. By
configuring and executing `cmake install` you obtain a directory with your ready-to-use binaries,
CMakes and libraries. The output of the `cmake install` command is what will be encapsulated inside
your Conan package.

## Configuring the Conanfile

To create a Conan package, the `conanfile.py` must implement the `package()` function which, thanks
to your CMake install setup, will be as simple as it follows.

```python
def package(self):
  cmake = CMake(self)
  cmake.install()
```

Once the `package()` function is defined in your Conanfile, `conan create` builds the project and
packages the result in one step. This is what Sen's own CI runs:

```shell
conan create . --user your_user --channel your_channel
```

If you have already built the project and only want to package what is in the build folder, use
`conan export-pkg` instead:

```shell
conan export-pkg . --user your_user --channel your_channel
```

To upload it to a configured Conan remote:

```shell
conan upload "package_name/*" -r remote_name --confirm
```

Where `package_name` is the name your recipe declares and `remote_name` is the remote the package
will be stored in. Log in to that remote first, if you have not already:

```shell
conan remote login remote_name your_user -p your_password
```

### Automating package creation in CI

Sen builds its own Conan package from GitHub Actions, in `.github/workflows/conan.yaml`, a worked
example of the build and packaging steps. The shape is:

- Build and package with `conan create`, passing `--user` and `--channel` to set the reference
  coordinates and `--lockfile-out` so the exact dependency set used is recorded.
- Restore and save the Conan cache around the build, which is what keeps a cold run from rebuilding
  every dependency each time.

Publishing is planned but not yet in place. The push step is a placeholder, and Sen is consumed from
the repository rather than from a remote. To publish a package of your own, add the login and upload
steps shown above to a job, keeping the credentials in repository secrets.
