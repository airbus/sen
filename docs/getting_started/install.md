![Screenshot](../assets/images/tools_light.svg#only-light){: style="width:150px; float: right;"}
![Screenshot](../assets/images/tools_dark.svg#only-dark){: style="width:150px; float: right;"}

# Getting Sen

## Using Conan

Sen releases are hosted on Conan-Center. To get it:

1. Create Conan configuration file (either _conanfile.txt_ or _conanfile.py_) in your project's
   top-level directory and add **Sen** as a dependency of your project.

2. Download, build, and install Conan dependencies before running the CMake configuration step:

```shell
conan build . --profile <your_conan_profile> --build=missing
```

??? info "Conan Set-Up"

    Install or upgrade Conan with:

    ```shell
    pip install -U conan
    ```

    Then create a profile for your environment in `<HOME>/.conan2/profiles`. Example:

    ```ini title="~/.conan2/profiles/gcc15"
    [settings]
    arch=x86_64
    build_type=Release
    compiler=gcc
    compiler.cppstd=17
    compiler.libcxx=libstdc++11
    compiler.version=15
    os=Linux

    [conf]
    tools.build:compiler_executables={"c": "gcc-15", "cpp": "g++-15"}
    ```

    As generator, we recommend Ninja. You can set it in `<HOME>/.conan2/global.conf`:

    ```text title="~/.conan2/global.conf"
    tools.cmake.cmaketoolchain:generator="Ninja Multi-Config"
    ```

    You can find some commonly-used conan profiles in the `.conan/profiles` folder. Those can be
    installed by running:

    ```shell
    conan config install -tf profiles .conan/profiles/<profile>
    ```

??? note "Example Conanfile"

    === "_conanfile.txt_"

        ```ini
        [requires]
        sen/1.0.0

        [layout]
        cmake_layout

        [generators]
        CMakeToolchain
        CMakeDeps
        ```

    === "_conanfile.py_"

        ```python
        from conan import ConanFile
        from conan.tools.cmake import CMake, cmake_layout

        class ProjectConfig(ConanFile):
            settings = "os", "arch", "compiler", "build_type"
            generators = "CMakeDeps", "CMakeToolchain"

            def requirements(self):
                self.requires("sen/x.y.z")

            def layout(self):
                cmake_layout(self)

            def build(self):
                cmake = CMake(self)
                cmake.configure()
                cmake.build()
        ```

## Using Release Packages

We provide binary releases in zip packages that you can download. You can extract the files into a
folder (called `<sen_path>` in the next examples).

To ensure your system is able to find all the paths, you can do as follows:

=== "Linux"

    ```shell
    export SEN_PATH=<sen_path>

    # Make the Sen binaries available
    export PATH=$SEN_PATH/bin:$PATH

    # Make the Sen libraries available
    export LD_LIBRARY_PATH=$SEN_PATH/bin:$LD_LIBRARY_PATH
    export DYLD_LIBRARY_PATH=$SEN_PATH/bin:$DYLD_LIBRARY_PATH
    ```

=== "Windows"

    ```shell
    set SEN_PATH=<sen_path>

    # Make the Sen binaries available
    set PATH=%SEN_PATH%/bin;%PATH%
    ```

In your CMakeLists.txt file, you would then set the path so that it can find Sen.

```cmake
list(APPEND CMAKE_PREFIX_PATH "$ENV{SEN_PATH}/cmake")

find_package(sen REQUIRED)
```

??? note "Helper script"

    For Linux, we offer a script that automates the Sen installation (needs wget and curl).

    Paste this in your shell:

    ```shell
    wget -qO- https://github.com/airbus/sen/releases/download/x.y.z/setup.sh | sh
    ```

    ??? note "Example"

        The process should look more or less like this:

        ```sh
        $ wget -qO- https://github.com/airbus/sen/releases/download/x.y.z/setup.sh | sh

        Sen installer script

        Configuration
        > Version:    x.y.z
        > Platform:   linux-x86_64
        > Directory:  /home/<user name>/.sen
        > Build:      release
        > Compiler:   All

        ? Install Sen x.y.z to /home/<user name>/.sen? [y/N] y

        Installation
        ✓ Downloaded sen-x.y.z-x86_64-linux-gnu-a.b.c-release.
        ✓ Unpacked.

        > Setup your fish environment with:
          source /home/<user name>/.sen/sen-x.y.z-x86_64-linux-gnu-a.b.c-release/setup
        ```

        The installation will create a script that you can use to set up your environment.
        For example:

        ```sh
        $ source /home/<user name>/.sen/sen-x.y.z-x86_64-linux-gnu-a.b.c-release/setup
        > environment configured for sen-x.y.z-x86_64-linux-gnu-a.b.c-release
        ```

        You should be able to do this now:

        ```text
        $ sen --version
        x.y.z
        ```

## Building Sen

**Sen** requires at least C++17.

Use Conan to fetch the third-party dependencies `conan install . --profile=sen_gcc --build=missing`
(you can replace 'sen_gcc' with the preset of your choice).

To build, use `conan build . --profile=sen_gcc`. Alternatively, use
`cmake -S . -B build -G Ninja --preset sen_gcc && cmake --build build` (you can replace 'sen_gcc'
with the preset of your choice).

??? note "Build options"

    **Component selection**

     `mode` is the Conan-level switch. It picks which components compile and which deps Conan fetches.

     | Mode        | Components enabled                                                                |
     | ----------- | --------------------------------------------------------------------------------- |
     | `barebones` | none - libs only, for embedding Sen as a library                                  |
     | `basic`     | `shell`, `ether` (minimum interactive set)                                        |
     | `full`      | every component (default)                                                         |

     ```shell
     conan install . --profile=sen_gcc -o sen/*:mode=barebones --build=missing
     conan install . --profile=sen_gcc -o sen/*:mode=basic --build=missing
     ```

     If in your local build you want to disable or enable specific components, use the CMake
     override `-DSEN_BUILD_<component>=OFF` or `-DSEN_BUILD_<component>=ON`. You can also use
     ccmake to set these flags.

     ```shell
     conan install . --profile=sen_gcc --build=missing
     cmake --preset conan-gcc-release -DSEN_BUILD_TRACY=OFF -DSEN_BUILD_EXPLORER=OFF
     ```

     The CMake override doesn't change what Conan fetched. Components beyond `mode`
     can't be enabled this way (their deps weren't fetched).

     **Developer-facing flags**

     Everything about *how* the tree is compiled is a CMake flag, applied at the configure step
     on top of the Conan-generated preset. All default to off.

     | Flag                           | Effect                            |
     | ------------------------------ | --------------------------------- |
     | `-DSEN_BUILD_EXAMPLES=ON`      | compile the examples              |
     | `-DSEN_BUILD_TESTS=ON`         | compile the test suite            |
     | `-DSEN_DISABLE_CLANG_TIDY=OFF` | run clang-tidy while compiling    |
     | `-DSEN_COVERAGE_ENABLE=ON`     | instrument for coverage           |
     | `-DSEN_USE_SANITIZER=<x>`      | `None`, `ASanUBSan`, or `Thread`  |

     ```shell
     # Configure a build that compiles the test suite with the address sanitizer
     conan install . --profile=sen_gcc --build=missing
     cmake --preset conan-gcc-release -DSEN_BUILD_TESTS=ON -DSEN_USE_SANITIZER=ASanUBSan
     cmake --build build/gcc/Release
     ```

     The flags override the preset defaults for your build tree. Note that explicitly re-running
     `cmake --preset ...` re-applies the preset defaults, so pass your `-D` flags again then
     (automatic re-configures during incremental builds keep them).

     Sanitized and instrumented builds are working-tree builds: `conan create` never passes
     `-D` flags, so a packaged Sen is always a plain build of its `mode`.

     **Building the docs**

     Docs need `doxygen`, which is a Conan-level concern (a tool requirement). It needs a
     `user.sen:build_docs` conf rather than an option, and `doxygen` itself needs `compiler.cppstd=20`
     per-dep, which must come from a profile. Sen ships the `sen_build_docs` profile that sets both, so
     the one-liner for docs is:

     ```shell
     conan install . --profile=sen_build_docs --build=missing
     ```

     `mkdocs` and `graphviz` are not Conan-managed - install them via `pip install -r docs/requirements.txt`
     and your platform package manager.
