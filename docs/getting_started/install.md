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

Install the profiles with `conan config install -tf profiles .conan/profiles/`, then fetch the
third-party dependencies with `conan install . --profile=sen_gcc_x86 --build=missing`. Use
`sen_gcc_arm` on arm hardware: the profiles pin the architecture, and the wrong one fails while
installing system libraries.

To build, use `conan build . --profile=sen_gcc_x86`. Alternatively drive CMake yourself with the
preset Conan generated: `cmake --preset conan-gcc-release && cmake --build --preset conan-gcc-release`.

??? note "Build options"

    **Component selection**

     `mode` is the Conan-level switch; it picks which components compile and which
     deps Conan fetches.

     | Mode        | Components enabled                                                                |
     | ----------- | --------------------------------------------------------------------------------- |
     | `barebones` | none - libs only, for embedding Sen as a library                                  |
     | `basic`     | `shell`, `ether` (minimum interactive set)                                        |
     | `full`      | every component (default)                                                         |

     ```shell
     conan install . --profile=sen_gcc -o sen/*:mode=barebones --build=missing
     conan install . --profile=sen_gcc -o sen/*:mode=basic --build=missing
     ```

     Per-component Conan options are deliberately not exposed (combinatorial
     package_id). Developers skip building specific components at the CMake step:

     ```shell
     conan install . --profile=sen_gcc --build=missing
     cmake --preset conan-gcc-release -DSEN_BUILD_TRACY=OFF -DSEN_BUILD_EXPLORER=OFF
     ```

     The CMake override doesn't change what Conan fetched. Components beyond `mode`
     can't be enabled this way (their deps weren't fetched).

     **Developer-facing flags**

     Examples, tests, static analysis, coverage, sanitizers, and documentation are exposed as Conan
     options. All default to off. Turn on what you need with `-o sen/*:…=True`.

     | Option            | Default  | Maps to                                                                       |
     | ----------------- | -------- | ----------------------------------------------------------------------------- |
     | `with_examples`   | `False`  | `-DSEN_BUILD_EXAMPLES=ON`                                                     |
     | `with_tests`      | `False`  | `-DSEN_BUILD_TESTS=ON`                                                        |
     | `with_clang_tidy` | `False`  | `-DSEN_DISABLE_CLANG_TIDY=OFF` (polarity flipped)                             |
     | `with_coverage`   | `False`  | `-DSEN_COVERAGE_ENABLE=ON`                                                    |
     | `with_docs`       | `False`  | `-DSEN_BUILD_DOCS=ON` and pulls `doxygen` as a tool requirement               |
     | `sanitizer`       | `"none"` | `-DSEN_USE_SANITIZER=None`/`ASanUBSan`/`Thread` for `none`/`address`/`thread` |

     Options are applied at `conan install` time — that's the step that generates the build files. The
     subsequent `conan build` (or a direct `cmake --build`) just compiles with the settings already baked
     in; the `-D` mappings above are for users invoking CMake without Conan.

     ```shell
     # Configure a build that compiles the test suite with the address sanitizer
     conan install . --profile=sen_gcc -o sen/*:with_tests=True -o sen/*:sanitizer=address --build=missing
     ```

     **Building the docs**

     `with_docs=True` pulls `doxygen` automatically, but `doxygen` itself needs `compiler.cppstd=20`
     (set per-dep), which has to come from a profile rather than from the recipe. Sen ships a
     `sen_build_docs` profile that sets both, so the one-liner for docs is:

     ```shell
     conan install . --profile=sen_build_docs --build=missing
     ```

     `mkdocs` and `graphviz` are not Conan-managed - install them via `pip install -r docs/requirements.txt`
     and your platform package manager.
