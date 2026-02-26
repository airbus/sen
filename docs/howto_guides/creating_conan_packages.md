# Creating your own Conan package

## Conanfile

The Conan configuration of a project resides on the conanfile.py. This file should be placed in the
root of the repository. It contains the necessary configuration, data and functions to make Conan
fetch dependencies and create your package.

## Packages and libraries

As +previously mentioned, in Sen you write packages. Packages can be loaded to create objects and be
run on components, so they are perfect candidates for Conan. To consume a Sen package, you will only
need the dynamic libraries (.so in Linux, .dll in Windows).

You might also want to export and import libraries or executables as Conan packages.

Exporting packages and libraries to a Conan package requires configuring the installation of the
desired binaries and headers using CMake install() function.

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
├── ...
```

In this CMake dir, notice that there are two shown files under the `util` directory: `install.cmake`
and `myproject-config.cmake.in`.

The `myproject-config.cmake.in` file ensures that libraries and packages exported through Conan can
be located when using CMake's find_package() command and are visible to consumers.

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
- In the configuration of install directories section, you will need to add where do you want the
  CMake files to be installed. Typically, this will be `cmake/your_project_name`. Any additional
  directory can also be added in this section (Sen adds examples here).
- The export targets section defines the file where the targets will be exported. This file is
  critical, since it will be the one read by any external CMake that consumes your project as a
  Conan package. Ensure to comply with naming and add the namespace defined at the beginning of the
  install.cmake file.
- The configuration file for CMake-based user consumption section will generate CMake's package
  configuration based on the contents of `mypackage-config.cmake.in`. It will generate versioning
  information inside `myPackageConfigVersion.cmake`, as well as the `config.cmake` file.
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
will be found. The `BUILD` interface should use `CMAKE_PROJECT_DIR` as reference, whilst the
`INSTALL` interface will be relative to the install root directory of the project.

### Exporting auto-generated headers

In Sen-based libraries, code can be auto-generated out of STLs or HLA FOMs. There are some
applications where the Conan package should contain the **already compiled** files from STL, not the
STL files themselves. This can come in handy when the developer consuming the Conan package may not
need to generate code. When generating C++ code from STL files using Sen CMake macros such as
`sen_generate_cpp()` or `sen_generate_package()`, we can use the `GEN_HDR_FILES` argument to specify
a variable with the list of C++ generated files. This list will contain the path to each file, so we
can use it in the `install()` macro to copy the output files to our install directory, which will be
later packed inside our Conan package. For example,

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
configuring and executing `cmake install` we can obtain a directory with our ready-to-use binaries,
CMakes and libraries. The output of the `cmake install` command is what will be encapsulated inside
our Conan package.

## Configuring the Conanfile

To create a Conan package, the `conanfile.py` must implement the `package()` function which, thanks
to our CMake install setup, will be as simple as it follows.

```python
def package(self):
  cmake = CMake(self)
  cmake.install()
```

Once the `package()` function is defined in your Conanfile, you only need to run the
`conan export-pkg` command.

```shell
conan export-pkg -f -if build_directory .
```

When executed on the root of the project, this command will generate the package based on the
contents of the build directory. In order to upload the package to a configured Conan remote,

```shell
conan upload package_name --all -c -r remote_name
```

Where package_name corresponds to the name of the project defined on the `install.cmake` file, and
`remote_name` is the target remote where the package will be stored.

### Configuring a Jenkins pipeline to automate package creation

If your project is already running a Jenkins pipeline for CI/CD check, you might be interested in
adding the feature of building and uploading the Conan package automatically. This is done in Sen,
so you can take Sen's JenkinsFile. as a reference. In a nutshell,

- Configure the Conan users for the remote where the package will be uploaded:

```shell
conan user $SEN_BUILD_CRED_USR -p $SEN_BUILD_CRED_PSW -r sim-csr-conan
```

NOTE: this step should be already performed in your JenkinsFile if you are already consuming any
dependency from Conan.

- Add a package creation step where the `conan export-pkg` command is executed. Sen uses Makefiles
  for everything related to Jenkins.

```shell
stage('Package')
{
  steps {
    container('env') {
      sh 'cd .jenkins && make package BUILD_TYPE=$BUILD_TYPE $CONAN_REFERENCE'
    }
  }
}
```

The Conan reference is an added marked as `stable` or `devel`, depending on the build.

- Configure a package upload stage: in Sen's Jenkins configuration, the Sen Conan package is
  uploaded on every `master` build, as well as when a tag is set. Sen also creates a `latest` alias
  for the packages, which ensures that the pushed package is flagged as the latest version. When
  consuming Sen as a Conan package, you can write `"sen/(latest)@airbus/devel"` and the alias will
  be automatically resolved to the latest version uploaded.
