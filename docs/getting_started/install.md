![Screenshot](../assets/images/tools_light.svg#only-light){: style="width:150px; float: right;"}
![Screenshot](../assets/images/tools_dark.svg#only-dark){: style="width:150px; float: right;"}

# Getting Sen

There are three ways to get Sen, depending on what you want to do:

- **Try Sen quickly** on Linux without setting up Conan: use the [quick installer](#quick-install-linux).
- **Use Sen as a dependency in your project**: use the [Conan package](#using-sen-in-your-project-conan).
- **Install Sen on a machine without an internet-facing toolchain** (Windows, air-gapped Linux): use the
  [release zip packages](#manual-release-packages).

If you want to compile Sen yourself, see [Building Sen from source](../howto_guides/building_from_source.md).

## Quick install (Linux)

A single POSIX `sh` script. No Conan, no `sudo`, no system files touched.

**1. Install:**

```shell
curl -sSf https://raw.githubusercontent.com/airbus/sen/main/resources/installer/install.sh | sh -s -- 0.5.2
```

**2. Activate** (and append the same line to your shell rc to load Sen on every new shell):

```shell
. ~/.sen/current/activate          # bash / zsh
source ~/.sen/current/activate.fish # fish
```

**3. Check:**

```shell
sen --version
```

??? note "What the installer prints"

    ```text
      Sen Installer  v0.2.0
      ▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬▬

      Configuration
        Version    0.5.2
        Toolchain  gcc 12.4.0
        Arch / OS  x86_64-linux
        Prefix     /home/alice/.sen/0.5.2-x86_64-linux-gcc-12.4.0

      ✓ Downloaded sen-0.5.2-x86_64-linux-gcc-12.4.0-release.tar.gz (42M)
      ✓ Verified sha256 checksum
      ✓ Extracted into /home/alice/.sen/0.5.2-x86_64-linux-gcc-12.4.0
      ✓ Cached CLI completions  (bash, zsh, fish)
      ✓ Wrote integrity manifest
      ✓ Wrote activate scripts
      ✓ Refreshed cached installer
      ✓ Updated 'current' to  0.5.2-x86_64-linux-gcc-12.4.0

      ──────────────────────────────────────────────────────────────────────

      ✓ Sen 0.5.2-x86_64-linux-gcc-12.4.0 installed.

      Activate this build:
        bash/zsh   . /home/alice/.sen/current/activate
        fish       source /home/alice/.sen/current/activate.fish
    ```

??? note "Different versions, toolchains, non-interactive"

    Run with no arguments to list the available releases:

    ```shell
    curl -sSf .../install.sh | sh
    ```

    A release with multiple toolchains (gcc, clang, ...) opens an interactive menu. Skip it by pinning a
    toolchain explicitly, or run fully non-interactively:

    ```shell
    sh install.sh 0.5.2 --compiler gcc-12.4.0
    sh install.sh 0.5.2 --yes
    ```

??? note "Switching versions and pinning a specific build"

    `~/.sen/current` is a symlink to the most recently installed build. Running `sh install.sh <other-version>`
    flips the symlink, even if that version was already installed.

    To pin a specific build, source the per-build path directly instead of `current/`:

    ```shell
    . ~/.sen/0.5.2-x86_64-linux-gcc-12.4.0/activate
    ```

    The activate scripts strip any prior `~/.sen/`-rooted entries from `PATH` and friends, so re-sourcing or
    switching is idempotent.

??? note "What activate sets, and how to uninstall"

    Sourcing the activate file exports `SEN_PREFIX`, prepends the build's `bin/` to `PATH` and to
    `LD_LIBRARY_PATH`, and points `CMAKE_PREFIX_PATH` at the prefix so `find_package(sen)` works.

    To uninstall:

    ```shell
    rm -rf ~/.sen/<build-id>     # one build
    rm -rf ~/.sen                # everything
    ```

    Then drop the `source ...activate` line from your shell rc.

For environment variables, the security model, and the full set of options, see
[`resources/installer/architecture.md`](https://github.com/airbus/sen/blob/main/resources/installer/architecture.md).

## Using Sen in your project (Conan)

Sen ships as a Conan package. Publication on Conan-Center is on the roadmap; in the meantime the package is
consumed from the project's repository.

The recipe sets `cmake_find_mode = "none"` (in `conanfile.py`), so Conan does not generate a synthetic
`senConfig.cmake` for downstream consumers. Instead, your build picks up Sen's own
`<prefix>/cmake/sen/sen-config.cmake` via the `CMAKE_PREFIX_PATH` that `CMakeDeps` populates: `find_package(sen)`
"just works" once the toolchain file is loaded.

1. Add a Conan configuration file (`conanfile.txt` or `conanfile.py`) at the top level of your project and list
   **Sen** as a dependency.
2. Make sure you have a Conan profile that matches your host. If this is your first time using Conan, run
   `conan profile detect` once: it inspects your installed compiler, OS, and architecture and writes
   `~/.conan2/profiles/default`. Without a profile, the next step errors with `Profile 'default' doesn't exist`.
3. Resolve, build, and install the dependencies before running CMake:

   ```shell
   conan build . --profile <your_conan_profile> --build=missing
   ```

   Always pass `--build=missing`. Without it, Conan refuses to build any dependency that doesn't already have a
   matching binary in its cache, which is rarely what you want on a fresh checkout.

??? info "Conan set-up"

    Install or upgrade Conan with:

    ```shell
    pip install -U conan
    ```

    Create a profile for your environment in `<HOME>/.conan2/profiles`:

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

    Sen recommends Ninja Multi-Config as the CMake generator. Set it once in `<HOME>/.conan2/global.conf`:

    ```text title="~/.conan2/global.conf"
    tools.cmake.cmaketoolchain:generator="Ninja Multi-Config"
    ```

    The Sen repository ships ready-to-use profiles in `.conan/profiles` (`sen_gcc`, `sen_clang`, `sen_msvc`,
    `sen_build_docs`). They target Sen's CI baseline (Linux x86_64, gcc 12 / clang): useful if you need to
    reproduce a CI build, less so as a default. Install one with:

    ```shell
    conan config install -tf profiles .conan/profiles/<profile>
    ```

    For different compiler versions, prefer `conan profile detect` over the bundled profiles. See
    [Building Sen from source](../howto_guides/building_from_source.md) for the full walk-through.

??? note "Example conanfile"

    Replace `x.y.z` with the Sen version you want. The recipe derives its version from `git describe --tags`, so
    a tagged release reads as `0.5.2` while an in-between commit reads as `0.5.2-5-gc6625265` (5 commits past tag
    `0.5.2`, at hash `c6625265`).

    === "_conanfile.txt_"

        ```ini
        [requires]
        sen/x.y.z

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

## Manual release packages

For Windows or environments where the quick installer is not an option, download the release archive for your
platform from the [Releases page](https://github.com/airbus/sen/releases) and extract it anywhere. The extracted
directory is `<sen_path>` in the snippets below.

=== "Linux"

    ```shell
    export SEN_PREFIX=<sen_path>

    # Sen binaries on PATH. Sen installs binaries, shared libraries, and archives all under <prefix>/bin
    # (CMAKE_INSTALL_BINDIR), so the same directory goes on LD_LIBRARY_PATH.
    export PATH="$SEN_PREFIX/bin:$PATH"
    export LD_LIBRARY_PATH="$SEN_PREFIX/bin:$LD_LIBRARY_PATH"
    ```

=== "Windows"

    ```bat
    set SEN_PREFIX=<sen_path>

    rem Sen binaries and DLLs on PATH
    set PATH=%SEN_PREFIX%\bin;%PATH%
    ```

In your project's `CMakeLists.txt`, point CMake at the prefix and pull Sen in with `find_package`:

```cmake
list(APPEND CMAKE_PREFIX_PATH "$ENV{SEN_PREFIX}")
find_package(sen REQUIRED)
```

Sen installs its CMake config under `<prefix>/cmake/sen/sen-config.cmake`, which CMake's standard search picks up
from `<prefix>` directly (no `/cmake` suffix needed).

## Building from source

If you want to compile Sen yourself (to track `main`, patch the code, or run on a platform without a release
artifact), see [Building Sen from source](../howto_guides/building_from_source.md).
