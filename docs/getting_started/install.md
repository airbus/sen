![Screenshot](../assets/images/tools_light.svg#only-light){: style="width:150px; float: right;"}
![Screenshot](../assets/images/tools_dark.svg#only-dark){: style="width:150px; float: right;"}

# Getting Sen

How you get Sen depends on what you want to do:

- **Try Sen quickly** on Linux without setting up Conan: use the [quick
  installer](#quick-install-linux).
- **Use Sen as a dependency in your project**: use the [Conan
  package](#using-sen-in-your-project-conan).
- **Install Sen on a machine without an internet-facing toolchain** (Windows, air-gapped Linux): use
  the [release zip packages](#manual-release-packages).
- **Compile Sen yourself**, to track `main` or to run on a platform with no release artifact: see
  [building from source](#building-from-source).

## Quick install (Linux)

Sen provides an installer script that does this for you. It is plain POSIX `sh`, needs no Conan, and
installs under `~/.sen` rather than into system directories, refusing to run as root.

Releases publish `x86_64` Linux and `amd64` Windows archives only. The script picks the asset that
matches your host, so on any other architecture, arm64 Linux included, it finds nothing to download
and stops. Build [from source](../howto_guides/building_from_source.md) there.

**1. Install:**

```shell
curl -sSf https://raw.githubusercontent.com/airbus/sen/main/resources/installer/install.sh | sh -s -- 0.6.0
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
        Version    0.6.0
        Toolchain  gcc 12.4.0
        Arch / OS  x86_64-linux
        Prefix     /home/alice/.sen/0.6.0-x86_64-linux-gcc-12.4.0

      ✓ Downloaded sen-0.6.0-x86_64-linux-gcc-12.4.0-release.tar.gz (42M)
      ✓ Verified sha256 checksum
      ✓ Extracted into /home/alice/.sen/0.6.0-x86_64-linux-gcc-12.4.0
      ✓ Cached CLI completions  (bash, zsh, fish)
      ✓ Wrote integrity manifest
      ✓ Wrote activate scripts
      ✓ Refreshed cached installer
      ✓ Updated 'current' to  0.6.0-x86_64-linux-gcc-12.4.0

      ──────────────────────────────────────────────────────────────────────

      ✓ Sen 0.6.0-x86_64-linux-gcc-12.4.0 installed.

      Activate this build:
        bash/zsh   . /home/alice/.sen/current/activate
        fish       source /home/alice/.sen/current/activate.fish
    ```

??? note "Different versions, toolchains, non-interactive"

    Run with no arguments to list the available releases:

    ```shell
    curl -sSf .../install.sh | sh
    ```

    A release with multiple toolchains (gcc, clang, ...) opens an interactive menu. Skip it by
    pinning a toolchain explicitly, or run fully non-interactively:

    ```shell
    sh install.sh 0.6.0 --compiler gcc-12.4.0
    sh install.sh 0.6.0 --yes
    ```

??? note "Switching versions and pinning a specific build"

    `~/.sen/current` is a symlink to the most recently installed build. Running `sh install.sh
    <other-version>` flips the symlink, even if that version was already installed.

    To pin a specific build, source the per-build path directly instead of `current/`:

    ```shell
    . ~/.sen/0.6.0-x86_64-linux-gcc-12.4.0/activate
    ```

    The activate scripts strip any prior `~/.sen/`-rooted entries from `PATH` and friends, so
    re-sourcing or switching is idempotent.

??? note "What activate sets, and how to uninstall"

    Sourcing the activate file exports `SEN_PREFIX`, prepends the build's `bin/` to `PATH` and to
    `LD_LIBRARY_PATH`, and prepends `<prefix>/cmake` to `CMAKE_PREFIX_PATH` so `find_package(sen)`
    works. The `/cmake` suffix is the part that matters; see below.

    To uninstall:

    ```shell
    rm -rf ~/.sen/<build-id>     # one build
    rm -rf ~/.sen                # everything
    ```

    Then drop the `source ...activate` line from your shell rc.

For environment variables, the security model, and the full set of options, see
[`resources/installer/architecture.md`](https://github.com/airbus/sen/blob/main/resources/installer/architecture.md).

## Using Sen in your project (Conan)

Sen ships as a Conan package. Publication on Conan-Center is on the roadmap; until then there is no
remote to resolve it from, so you put it in your local Conan cache yourself:

```shell
git clone https://github.com/airbus/sen.git
cd sen
git checkout 0.6.0
conan create .
```

Check out the release tag first. The recipe takes its version from `git describe --tags`, and the
tags are on the `release/x.y.x` branches rather than on `main`, so a fresh clone left on `main`
gives you `sen/<commit hash>` instead of `sen/0.6.0`.

`conan create .` uses your default Conan profile. If you have never used Conan, run
`conan profile detect` once first. Sen's own profiles in `.conan/profiles/` are for building Sen
itself: they fix the operating system and pin compiler versions and executable names, so they are
not a starting point for your machine.

The recipe sets `cmake_find_mode = "none"` (in `conanfile.py`), so Conan does not generate a
synthetic `senConfig.cmake` for downstream consumers. Instead, your build picks up Sen's own
`<prefix>/cmake/sen/sen-config.cmake` via the `CMAKE_PREFIX_PATH` that `CMakeDeps` populates:
`find_package(sen)` "just works" once the toolchain file is loaded.

1. Add a Conan configuration file (`conanfile.txt` or `conanfile.py`) at the top level of your
   project and list **Sen** as a dependency.
2. Make sure you have a Conan profile that matches your host. If this is your first time using
   Conan, run `conan profile detect` once: it inspects your installed compiler, OS, and architecture
   and writes `~/.conan2/profiles/default`. Without a profile, the next step errors with `Profile
   'default' doesn't exist`.
3. Resolve, build, and install the dependencies before running CMake:

   ```shell
   conan install . --profile:all <your_conan_profile> --build=missing
   ```

   Always pass `--build=missing`. Without it, Conan refuses to build any dependency that doesn't
   already have a matching binary in its cache, which is rarely what you want on a fresh checkout.

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

    Sen recommends Ninja Multi-Config as the CMake generator. Set it once in
    `<HOME>/.conan2/global.conf`:

    ```text title="~/.conan2/global.conf"
    tools.cmake.cmaketoolchain:generator="Ninja Multi-Config"
    ```

    The Sen repository ships ready-to-use profiles in `.conan/profiles`. `sen_gcc`, `sen_clang` and
    `sen_msvc` pin the compilers CI builds with and otherwise follow the machine you run them on;
    the suffixed ones such as `sen_gcc_x86` and `sen_gcc_arm` name an architecture as well. They are
    useful for reproducing a CI build, less so as a default. Install them with:

    ```shell
    conan config install -tf profiles .conan/profiles/
    ```

    Install the whole folder, not one file. The three base profiles, `sen_gcc`, `sen_clang`
    and `sen_msvc`, are self-contained and do work on their own. The other five, the four
    architecture variants plus `sen_build_docs`, are an `include` of their base plus what they
    override, so installing `sen_gcc_x86` by itself fails with `Profile not found: sen_gcc`. Note
    that the error names the base it could not find, not the profile you asked for.

    For different compiler versions, prefer `conan profile detect` over the bundled profiles. See
    [Building Sen from source](../howto_guides/building_from_source.md) for the full walk-through.

??? note "Example conanfile"

    Replace `x.y.z` with the Sen version you want. The recipe derives its version from `git describe
    --tags`, so a tagged release reads as `0.6.0` while an in-between commit reads as
    `0.6.0-5-gc6625265` (5 commits past tag `0.6.0`, at hash `c6625265`).

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

For Windows or environments where the quick installer is not an option, download the release archive
for your platform from the [Releases page](https://github.com/airbus/sen/releases) and extract it
anywhere. The extracted directory is `<sen_path>` in the snippets below.

=== "Linux"

    ```shell
    export SEN_PREFIX=<sen_path>

    # Sen binaries on PATH. Sen finds its own shared libraries through its run path, so this is all
    # that running sen needs.
    export PATH="$SEN_PREFIX/bin:$PATH"
    ```

    An application you build against Sen has to find those libraries itself. Give it a run path of
    its own, or tell the loader where to look:

    ```shell
    # Sen installs binaries, shared libraries and archives all under <prefix>/bin
    # (CMAKE_INSTALL_BINDIR), so that is the directory to name.
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
list(APPEND CMAKE_PREFIX_PATH "$ENV{SEN_PREFIX}/cmake")
find_package(sen REQUIRED)
```

Sen installs its CMake config under `<prefix>/cmake/sen/sen-config.cmake`. CMake's standard search
does not reach that from `<prefix>` alone, so the `/cmake` suffix is required.

## Building from source

If you want to compile Sen yourself (to track `main`, patch the code, or run on a platform without a
release artifact), see [Building Sen from source](../howto_guides/building_from_source.md).

If your editor supports devcontainers, the repository ships one under `.devcontainer/`. It builds
the same environment the pipeline uses, from `tools/ci/Dockerfile`, so you do not have to install
compilers or Conan yourself.

??? note "Build options"

    **Component selection**

     `mode` is the Conan-level switch; it picks which components compile and which
     deps Conan fetches.

     | Mode        | Components enabled                                                                |
     | ----------- | --------------------------------------------------------------------------------- |
     | `barebones` | none (libs only, for embedding Sen as a library)                                   |
     | `basic`     | `shell`, `ether` (minimum interactive set)                                         |
     | `full`      | every component (default)                                                         |

     ```shell
     conan install . --profile:all=sen_gcc -o sen/*:mode=barebones --build=missing
     conan install . --profile:all=sen_gcc -o sen/*:mode=basic --build=missing
     ```

     Per-component Conan options are deliberately not exposed (combinatorial
     package_id). Developers skip building specific components at the CMake step:

     ```shell
     conan install . --profile:all=sen_gcc --build=missing
     cmake --preset conan-gcc-release -DSEN_BUILD_TRACY=OFF -DSEN_BUILD_EXPLORER=OFF
     ```

     The CMake override doesn't change what Conan fetched. Components beyond `mode`
     can't be enabled this way (their deps weren't fetched).

     **Developer-facing flags**

     Examples, tests, static analysis, coverage, sanitizers, and documentation are exposed as Conan
     options. All default to off. Turn on what you need with `-o sen/*:…=True`.

     | Option            | Default  | Maps to |
     | ----------------- | -------- | ----------------------------------------------------------------------------- |
     | `with_examples`   | `False`  | `-DSEN_BUILD_EXAMPLES=ON` |
     | `with_tests`      | `False`  | `-DSEN_BUILD_TESTS=ON` |
     | `with_clang_tidy` | `False`  | `-DSEN_DISABLE_CLANG_TIDY=OFF` (polarity flipped) |
     | `with_coverage`   | `False`  | `-DSEN_COVERAGE_ENABLE=ON` |
     | `with_docs`       | `False`  | `-DSEN_BUILD_DOCS=ON` and pulls `doxygen` as a tool requirement               |
     | `sanitizer`       | `"none"` | `-DSEN_USE_SANITIZER=None`/`ASanUBSan`/`Thread` for `none`/`address`/`thread` |

     Options are applied at `conan install` time, the step that generates the build files. The
     subsequent `conan build` (or a direct `cmake --build`) just compiles with the settings already
     baked in; the `-D` mappings above are for users invoking CMake without Conan.

     ```shell
     # Configure a build that compiles the test suite with the address sanitizer
     conan install . --profile:all=sen_gcc -o sen/*:with_tests=True -o sen/*:sanitizer=address --build=missing
     ```

     **Building the docs**

     `with_docs=True` pulls `doxygen` automatically, but `doxygen` itself needs `compiler.cppstd=20`
     (set per-dep), which has to come from a profile rather than from the recipe. Sen ships a
     `sen_build_docs` profile that sets both, so the one-liner for docs is:

     ```shell
     conan install . --profile:all=sen_build_docs --build=missing
     ```

     `mkdocs` and `graphviz` are not Conan-managed, so install them via `pip install -r
     docs/requirements.txt` and your platform package manager.

??? note "What the build needs (toolchain, network, time)"

    **Toolchain.** Sen's own build gets its tools as Conan tool requirements: CMake, Ninja,
    GTest, and Node.js 22 whenever the `jsonrpc` component is enabled (any mode above `basic`),
    because the build generates the `@sen/client` TypeScript types, installs its npm
    dependencies, and bakes the web explorer bundle into the binary. Building the third-party
    packages from source is different: their recipes use the system's `cmake` and `pkg-config`,
    so have both installed before the first `conan install`.
    Don't install Node for the build; the pinned toolchain version comes with `conan install`.
    (The TS packages' *dev loops*, `npm run dev` and `vitest` on the host, do use your own
    Node >= 22; see `components/jsonrpc/clients/typescript/README.md`.)

    **Network.** The first `conan install`/`conan build` fetches from Conan Center **and**, for
    the browser stack, from the npm registry during the build itself (`npm ci`). Behind a
    proxy, make both reachable, or skip the web stack entirely: build `-o "sen/*:mode=basic"`,
    or stay in `full` mode and pass `-DSEN_BUILD_JSONRPC_TS_CLIENT=OFF -DSEN_BUILD_WEBEXPLORER=OFF`
    at the CMake step.

    **Time.** The first full-mode build compiles every third-party dependency plus the whole
    tree; on a typical developer machine expect on the order of half an hour to an hour.
    Subsequent builds are incremental. `ccache` shortens rebuilds. CI simply prepends the
    ccache masquerade directory to `PATH` before building.

    **Windows.** The C++ tree and the browser stack both build with MSVC, and projects run on it.
    The automated test suite has not been wired up for Windows since the move to GitHub, so the
    coverage that runs on Linux does not yet run there.

    For enabling and running the test suite, see [Running the tests](testing.md).

## Next steps

Sen is installed. The quickest way to see it working is to generate a package, build it and run it,
which both routes below walk through.

- **[The tutorials](../tutorials/index.md)**: Tutorial 1 goes from `sen package init` to an object
  you can watch changing in the shell.
- **[Create your first package](first_package.md)**: the same ground as reference, explaining what
  `sen package init` generates and what each file is for.
