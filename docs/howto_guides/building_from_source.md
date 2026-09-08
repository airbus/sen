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

When you go through the Conan-driven path below, Conan downloads CMake (3.31.10) and Ninja (1.13.2)
into its own cache as `tool_requires`: no system install needed for those.

**Required only if you skip Conan**

- [CMake](https://cmake.org/) 3.21 or newer (the project minimum).
- [Ninja](https://ninja-build.org/) (other generators work, but the bundled profiles assume Ninja).

Clone the repository:

```shell
git clone https://github.com/airbus/sen.git
cd sen
```

## Conan-driven build (recommended)

**Step 1: make sure you have a Conan profile.** The first time you use Conan, run:

```shell
conan profile detect
```

It inspects your installed compiler, OS, and architecture and writes `~/.conan2/profiles/default`.
Subsequent builds reuse it. If you skip this step, the next command errors with `Profile 'default'
doesn't exist`.

**Step 2: resolve dependencies and build:**

```shell
conan install . --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
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

## Direct CMake build

Once `conan install` has populated the build folder with toolchain files, you can drive CMake
directly. This is useful when iterating on Sen itself without re-running Conan each time.

```shell
conan install . --build=missing                     # generates the per-profile preset under build/
cmake --preset conan-release                         # configures the build
cmake --build --preset conan-release                 # compiles
```

The preset name is **not** the same as the Conan profile name. Conan generates presets named
`conan-<compiler>-<build_type>` when the profile sets `tools.cmake.cmake_layout:build_folder_vars`,
and just `conan-<build_type>` otherwise.

## Using the Sen you just built

A package generated by `sen package init` does `find_package(sen REQUIRED)` and looks under
`$SEN_PREFIX`'s CMake directories. A build tree has no such layout, so pointing `SEN_PREFIX` at
`build/gcc/Release` does not work. **Install it first:**

```shell
cmake --install build/gcc/Release --prefix ~/sen-local
export SEN_PREFIX=~/sen-local
```

From there every command in [Create your first package](../getting_started/first_package.md) and
in the tutorials works unchanged, because that is the layout they assume.

If `~/sen-local` already holds an older install, empty it first rather than installing on top. An
older release put third-party headers in `<prefix>/include`, and installing over it leaves both
copies there, where the stale one can win.

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
ctest --preset conan-release
```

To run a single test target by name pattern:

```shell
ctest --preset conan-release -R <pattern>
```

## Common first-run errors

| Error                                            | Fix                                                                                                                                                                                |
|--------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Profile 'default' doesn't exist`                | Run `conan profile detect` once.                                                                                                                                                   |
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
