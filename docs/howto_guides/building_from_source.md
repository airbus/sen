# Building Sen from source

Most users do not need to build Sen themselves; the [Conan package and the quick
installer](../getting_started/install.md) cover the common paths. Build from source when you want to track
`main`, patch Sen for local development, or run on a platform without a release artifact.

## Prerequisites

**Always required**

- A C++17 compiler (GCC 9.2.1+, Clang, or MSVC). Sen targets the C++17 language standard.
- [Conan](https://conan.io/) 2.x.

  ```shell
  pip install -U conan
  ```

- On Linux, a couple of system libraries that Conan can't ship (pulled in by SDL2 / imgui via Sen's
  `requirements()` block in `conanfile.py`). On Debian / Ubuntu:

  ```shell
  sudo apt install libxext-dev pkg-config
  ```

  Equivalents on RHEL / Fedora / SUSE: `libXext-devel` (or `libxext-devel`) plus `pkg-config`.
  Conan can also install these for you if you set `tools.system.package_manager:mode=install` in your profile,
  but pre-installing them is simpler and less intrusive.

When you go through the Conan-driven path below, Conan downloads CMake (3.28.1) and Ninja (1.13.2) into its own
cache as `tool_requires`: no system install needed for those.

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

It inspects your installed compiler, OS, and architecture and writes `~/.conan2/profiles/default`. Subsequent
builds reuse it. If you skip this step, the next command errors with `Profile 'default' doesn't exist`.

**Step 2: resolve dependencies and build:**

```shell
conan install . --build=missing
cmake --preset conan-gcc-release
cmake --build --preset conan-gcc-release
```

Always pass `--build=missing`. Without it, Conan refuses to build any dependency that doesn't already have a
matching binary in its cache.

The first run downloads and builds Sen's third-party dependencies (this can take a few minutes); subsequent runs
reuse Conan's cache.

The exact preset name depends on the detected compiler: `conan-gcc-release`, `conan-clang-release`, and so on.
Conan derives the name from `tools.cmake.cmake_layout:build_folder_vars` (set via the bundled profiles). If your
detected profile doesn't set that conf, the preset is just `conan-release`. List what's available with:

```shell
cmake --list-presets
```

??? note "Matching CI's exact toolchain"

    The repository ships ready-made profiles under `.conan/profiles/`:

    | Profile           | Target                                 |
    | ----------------- | -------------------------------------- |
    | `sen_gcc`         | Linux, gcc 12 (CI baseline)            |
    | `sen_clang`       | Linux, clang 20 (CI baseline)          |
    | `sen_msvc`        | Windows, MSVC                          |
    | `sen_build_docs`  | building the mkdocs documentation      |

    Install one and use it explicitly:

    ```shell
    conan config install -tf profiles .conan/profiles/sen_gcc
    conan install . --profile=sen_gcc --build=missing
    ```

    The `sen_gcc` and `sen_clang` profiles auto-detect architecture so the same profile resolves on x86_64 CI
    runners and aarch64 Linux hosts. They do not configure system-package installation; pre-install
    `libxext-dev` (see [Prerequisites](#prerequisites)) or pass `-c tools.system.package_manager:mode=install`
    on the `conan install` command line.

## Direct CMake build

Once `conan install` has populated the build folder with toolchain files, you can drive CMake directly. This is
useful when iterating on Sen itself without re-running Conan each time.

```shell
conan install . --build=missing                     # generates the per-profile preset under build/
cmake --preset conan-gcc-release                     # configures the build
cmake --build --preset conan-gcc-release             # compiles
```

The preset name is **not** the same as the Conan profile name. Conan generates presets named
`conan-<compiler>-<build_type>` when the profile sets `tools.cmake.cmake_layout:build_folder_vars`, and just
`conan-<build_type>` otherwise.

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
| `Profile 'default' doesn't exist`                | Run `conan profile detect` once.                                                                                                                                                   |
| `'libxext-dev' missing but can't install`        | Pre-install with `sudo apt install libxext-dev`, or pass `-c tools.system.package_manager:mode=install -c tools.system.package_manager:sudo=True` so Conan installs it via `sudo`. |
| `ERROR: Package '<dep>' missing prebuilt binary` | You forgot `--build=missing` on `conan install`.                                                                                                                                   |
| `compiler.version` setting mismatch              | Your profile pins a compiler version that isn't installed. Edit `~/.conan2/profiles/default` to match what's on `PATH`, or pass `--profile:build=<other>`.                         |

## Troubleshooting

- **First-build memory pressure**: building the dependency graph cold needs a few GB of RAM. Pass
  `--parallel <N>` to limit parallelism on constrained machines.

For other questions, the [Troubleshooting guide](troubleshooting.md) and the [FAQ](../users_guide/faq.md) cover
the common cases. Bug reports are welcome via the project's issue tracker.
