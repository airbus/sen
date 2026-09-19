# Building Sen from source

Most users do not need to build Sen themselves; the [Conan package and the quick
installer](../getting_started/install.md) cover the common paths. Build from source when you want to
track `main`, patch Sen for local development, or run on a platform without a release artifact.

## Prerequisites

**Always required**

- A C++17 compiler (GCC 9.2.1+, Clang, or MSVC). Sen targets the C++17 language standard. The CI
  builds with GCC 12, Clang 20 and MSVC 194; other versions are untested in the CI.
- [Conan](https://conan.io/) 2.x.

  ```shell
  pipx install conan
  pipx ensurepath
  ```

  Not `pip install`: current Debian and Ubuntu refuse it with
  `externally-managed-environment`. `ensurepath` needs a new shell to take effect.

- On Linux, `objdump` — from `binutils`. It is needed at **install** time, not build time: the
  install rules resolve which third-party shared objects the binaries actually need, and CMake uses
  `objdump` to read them. Without it `cmake --install` fails long after a successful build. It
  arrives with GCC, so a normal toolchain already has it; a minimal Conan-only image may not.

- On Linux, a couple of system libraries that Conan can't ship (pulled in by SDL2 / imgui via Sen's
  `requirements()` block in `conanfile.py`). On Debian / Ubuntu:

  ```shell
  sudo apt install libxext-dev pkg-config
  ```

  Equivalents on RHEL / Fedora / SUSE: `libXext-devel` (or `libxext-devel`) plus `pkg-config`. Conan
  can also install these for you if you set `tools.system.package_manager:mode=install` in your
  profile, but pre-installing them is simpler and less intrusive.

When you go through the Conan-driven path below, Conan downloads CMake (3.31.10), Ninja (1.13.2)
and GTest into its own cache as `tool_requires`, plus Node.js 22 whenever the `jsonrpc` component is
enabled, which is any mode above `basic`. None of those need a system install, and you should not install
Node yourself: the build uses the pinned one to generate the `@sen/client` TypeScript types, install
its npm dependencies and bake the web explorer bundle into the binary. The TypeScript packages' dev
loops, `npm run dev` and `vitest` on the host, do use your own Node 22 or newer; see
`components/jsonrpc/clients/typescript/README.md`.

Building the third-party packages from source is different: their recipes use the system's `cmake`
and `pkg-config`, so have both installed before the first `conan install`.

**Required only if you skip Conan**

- [CMake](https://cmake.org/) 3.21 or newer (the project minimum).
- [Ninja](https://ninja-build.org/) (other generators work, but the bundled profiles assume Ninja).

Clone the repository:

```shell
git clone https://github.com/airbus/sen.git
cd sen
```

## Conan-driven build (recommended)

**Step 1: install the profile Sen ships.** It pins the compiler, the C++ standard and Ninja, so
your build matches CI and matches the commands on the rest of these pages:

```shell
conan config install -tf profiles .conan/profiles/sen_gcc
```

Use `sen_clang` or `sen_msvc` if that is your compiler.

`conan profile detect` writes a profile describing your own machine and builds Sen too, but it
selects a different generator and different preset names, so the commands here and under [Running
the tests](../getting_started/testing.md) will not match what it produces.

**Step 2: resolve dependencies and build:**

```shell
conan install . --profile:all=sen_gcc --build=missing
cmake --preset conan-gcc-release
cmake --build --preset conan-gcc-release
```

Always pass `--build=missing`. Without it, Conan refuses to build any dependency that doesn't
already have a matching binary in its cache.

The first run downloads and builds Sen's third-party dependencies; subsequent runs reuse Conan's
cache.

The preset name comes from `tools.cmake.cmake_layout:build_folder_vars`, not from the compiler you
have. A detected profile does not set it, so the preset is `conan-release`. The bundled profiles do
set it, so building with one of those gives `conan-gcc-release`, `conan-clang-release` and so on.
List what you actually have with:

```shell
cmake --list-presets
```

??? note "Matching CI's exact toolchain"

    The repository ships ready-made profiles under `.conan/profiles/`:

    | Profile          | Target                                            |
    | ---------------- | ------------------------------------------------- |
    | `sen_gcc`        | Linux, gcc 12 (CI baseline)                       |
    | `sen_clang`      | Linux, clang 20 (CI baseline)                     |
    | `sen_msvc`       | Windows, MSVC 194                                 |
    | `sen_build_docs` | building the mkdocs documentation                 |
    | `sen_gcc_arm`    | `sen_gcc` with the architecture pinned to `armv8` |
    | `sen_gcc_x86`    | `sen_gcc`, unchanged                              |
    | `sen_clang_x86`  | `sen_clang`, unchanged                            |
    | `sen_msvc_x86`   | `sen_msvc`, unchanged                             |

    The four suffixed names exist for CI and the devcontainer. Each composes one from the compiler
    and the machine's architecture and copies it into place as the default profile, so a new
    combination in the matrix needs a profile of the matching name. The `README` and `CONTRIBUTING`
    give the unsuffixed `sen_gcc`, which is equivalent for a native build.

    Install one and use it explicitly:

    ```shell
    conan config install -tf profiles .conan/profiles/sen_gcc
    conan install . --profile:all=sen_gcc --build=missing
    ```

    That works for `sen_gcc`, `sen_clang` and `sen_msvc`, which are self-contained. The suffixed
    profiles are an `include` of one of those three, so install the whole folder
    (`.conan/profiles/`) if you want one of them.

    `--profile:all` sets both the host and the build profile. `--profile` sets only the host one,
    and Conan then looks for a build profile named `default`. A fresh checkout has none, and if you
    do have one it describes your own machine rather than the profile you asked for.

    The unsuffixed `sen_gcc` and `sen_clang` take the architecture of the machine you run them on,
    which makes them convenient for a native build. None of these profiles configure
    system-package installation; pre-install
    `libxext-dev` (see [Prerequisites](#prerequisites)) or pass `-c
    tools.system.package_manager:mode=install` on the `conan install` command line.

## Build options

`mode` is the Conan-level switch; it picks which components compile and which dependencies Conan
fetches.

| Mode        | Components enabled                                |
| ----------- | ------------------------------------------------- |
| `barebones` | none (libs only, for embedding Sen as a library)  |
| `basic`     | `shell`, `ether` (minimum interactive set)        |
| `full`      | every component (default)                         |

```shell
conan install . --profile:all=sen_gcc -o sen/*:mode=basic --build=missing
```

Per-component Conan options are deliberately not exposed, because they multiply the package ids.
Skip individual components at the CMake step instead:

```shell
conan install . --profile:all=sen_gcc --build=missing
cmake --preset conan-gcc-release -DSEN_BUILD_TRACY=OFF -DSEN_BUILD_EXPLORER=OFF
```

That does not change what Conan fetched, so components beyond the chosen `mode` cannot be turned on
this way: their dependencies are not there.

**Developer-facing flags.** Examples, tests, static analysis, coverage, sanitizers and documentation
are Conan options, all off by default.

| Option            | Default  | Maps to |
| ----------------- | -------- | ----------------------------------------------------------------------------- |
| `with_examples`   | `False`  | `-DSEN_BUILD_EXAMPLES=ON` |
| `with_tests`      | `False`  | `-DSEN_BUILD_TESTS=ON` |
| `with_clang_tidy` | `False`  | `-DSEN_DISABLE_CLANG_TIDY=OFF` (polarity flipped) |
| `with_coverage`   | `False`  | `-DSEN_COVERAGE_ENABLE=ON` |
| `with_docs`       | `False`  | `-DSEN_BUILD_DOCS=ON` and pulls `doxygen` as a tool requirement               |
| `sanitizer`       | `"none"` | `-DSEN_USE_SANITIZER=None`/`ASanUBSan`/`Thread` for `none`/`address`/`thread` |

They apply at `conan install`, the step that generates the build files. The later `conan build` or
`cmake --build` compiles with them already baked in; the `-D` column is for driving CMake without
Conan.

```shell
# tests compiled with the address sanitizer
conan install . --profile:all=sen_gcc -o sen/*:with_tests=True -o sen/*:sanitizer=address --build=missing
```

**Building the documentation.** `with_docs=True` pulls `doxygen`, which itself needs
`compiler.cppstd=20` set per-dependency, and that has to come from a profile rather than the
recipe. Sen ships one:

```shell
conan install . --profile:all=sen_build_docs --build=missing
```

`mkdocs` and `graphviz` are not Conan-managed. `graphviz` comes from your platform package manager;
the pinned Python set goes in a virtual environment, because current Debian and Ubuntu refuse to
install it into the system Python:

```shell
python3 -m venv .venv-docs
.venv-docs/bin/pip install -r docs/requirements.txt
```

A virtual environment rather than `pipx` because the file pins eleven plugins that `mkdocs` imports,
and they have to share its environment. `python3-venv` is a separate apt package and `python3 -m
venv` fails without it.

## What the build needs from the network, and how long it takes

The first `conan install` and `conan build` fetch from Conan Center **and**, for the browser stack,
from the npm registry during the build itself (`npm ci`). Behind a proxy, make both reachable, or
leave the web stack out with `-o "sen/*:mode=basic"`, or stay in `full` mode and pass
`-DSEN_BUILD_JSONRPC_TS_CLIENT=OFF -DSEN_BUILD_WEBEXPLORER=OFF -DSEN_BUILD_MCP_GATEWAY=OFF` at the
CMake step. The web explorer and the MCP gateway both bundle `@sen/client`, so each needs turning
off by name or CMake stops with an error.

A first full-mode build compiles every third-party dependency plus the whole tree: expect half an
hour to an hour. Later builds are incremental. Sen's own tree wires `ccache` in through
`CMAKE_<LANG>_COMPILER_LAUNCHER` whenever it is installed, with nothing to configure. Dependencies
build inside their own projects and never see that, so to cache those too, put your distribution's
ccache shim directory (`/usr/lib/ccache` on Debian and Ubuntu) ahead of the compilers on `PATH`
for the `conan install` step only, which is what CI does.

On Windows the C++ tree and the browser stack both build with MSVC, and projects run on it. The test
suite runs there too, on fewer configurations than Linux.

## Direct CMake build

Once `conan install` has populated the build folder with toolchain files, you can drive CMake
directly. This is useful when iterating on Sen itself without re-running Conan each time.

```shell
conan install . --profile:all=sen_gcc --build=missing  # generates the preset under build/
cmake --preset conan-gcc-release                       # configures the build
cmake --build --preset conan-gcc-release               # compiles
```

The preset name is **not** the same as the Conan profile name. Conan generates presets named
`conan-<compiler>-<build_type>` when the profile sets `tools.cmake.cmake_layout:build_folder_vars`,
and just `conan-<build_type>` otherwise.

## Using the Sen you just built

A package generated by `sen package init` does `find_package(sen REQUIRED)` and looks under
`$SEN_PREFIX/cmake`. A build tree has no such layout, so pointing `SEN_PREFIX` at
`build/gcc/Release` does not work. **Install it first:**

```shell
cmake --install build/gcc/Release --prefix ~/sen-local
export SEN_PREFIX=~/sen-local
```

From there every command in [Create your first package](../getting_started/first_package.md) and
in the tutorials works unchanged, because that is the layout they assume.

??? note "Configuring against the build tree without installing"

    It can be done, but the generated `CMakeLists.txt` does not supply everything. Conan's
    generated dependency configs have to be on the prefix path, and `CMAKE_BUILD_TYPE` has to be set
    explicitly, because Conan's `cmakedeps_macros.cmake` refuses to pick a configuration for you:

    ```shell
    cmake -S . -B build -G Ninja \
      -DCMAKE_PREFIX_PATH="$SEN_BUILD;$SEN_BUILD/generators" \
      -DCMAKE_BUILD_TYPE=Release
    ```

    Without the `generators` entry the configure fails on `find_dependency(spdlog)`, which
    `sen-config.cmake` calls. Without `CMAKE_BUILD_TYPE` it fails with "Please, set the
    CMAKE_BUILD_TYPE variable".

    Installing to a prefix is simpler and is what the rest of the documentation assumes. This is
    here for the case where you are iterating on Sen itself and do not want an install step in the
    loop.

## Building against a local Sen (Conan editable mode)

Editable mode lets a project that depends on Sen build against your working copy, without running
`conan create` after every change.

Add your checkout to Conan's editable list. The version has to match the one the consumer project
requires:

```shell
conan_channel=$([ -n "$TAG_NAME" ] && echo "stable" || echo "devel")
conan editable add . --user=airbus --channel=$conan_channel
```

`conan editable list` shows what is currently registered. Then run `conan install` in the consumer
project: Sen appears as `Editable` in the dependency list, and the consumer compiles against your
local build. To go back to the packaged version:

```shell
conan editable remove .
```

## Running the tests

```shell
ctest --preset conan-gcc-release
```

To run a single test target by name pattern:

```shell
ctest --preset conan-gcc-release -R <pattern>
```

## Common first-run errors

| Error                                            | Fix                                                                                                                                                                                |
|--------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Profile 'default' doesn't exist`                | Install a profile: `conan config install -tf profiles .conan/profiles/sen_gcc`.                                                                                                                                                   |
| `No such preset in CMakePresets.json`            | The preset name follows the profile, not the compiler. A detected profile gives `conan-release`; the bundled profiles give `conan-gcc-release` and the like. `cmake --list-presets` shows what you have. |
| `'libxext-dev' missing but can't install`        | Pre-install with `sudo apt install libxext-dev`, or pass `-c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True` so Conan installs it via `sudo`. |
| `ERROR: Package '<dep>' missing prebuilt binary` | You forgot `--build=missing` on `conan install`.                                                                                                                                   |
| `compiler.version` setting mismatch              | Your profile pins a compiler version that isn't installed. Edit `~/.conan2/profiles/default` to match what's on `PATH`, or pass `--profile:build=<other>`.                         |
| `VERSION "v0.6.0" format invalid.`               | The version tag carries no `v` prefix. Tag `0.6.0`, not `v0.6.0`. Configure exits 1 on the prefixed form.                                                                          |

## What `sen --version` reports from a source build

A binary you built yourself prints a bare commit hash, not a version number:

```shell
$ sen --version
0271e1ba
```

That is correct behavior, not a broken build. The string is fixed at configure time from
`git describe --tags --always`, so it reports a version only when a tag is reachable from the
commit you built. Sen's release tags live on the `release/*` branches and not on `main`, so
nothing built from `main` has one, and `--always` falls back to the hash. A release you install
through the [installer](../getting_started/install.md) prints `0.6.0`, because it was built from
a tagged commit.

There is no flag that changes this. If you need a build to report a version, build from a tag.

## Troubleshooting

- **First-build memory pressure**: building the dependency graph cold needs a few GB of RAM, and
  compiling defaults to one job per CPU. Add `-c tools.build:jobs=<N>` to `conan install` and
  `conan build` to limit that on a constrained machine.

For other questions, the [Troubleshooting guide](troubleshooting.md) and the
[FAQ](../users_guide/faq.md) cover the common cases. Bug reports are welcome via the project's issue
tracker.
