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
    pipx install conan
    pipx ensurepath
    ```

    `pipx` rather than `pip`: current Debian and Ubuntu refuse `pip install` into the system
    Python with `externally-managed-environment`. `ensurepath` puts `~/.local/bin` on `PATH`,
    and it takes a new shell before `conan` resolves. Later, `pipx upgrade conan`.

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

    The Sen repository ships ready-to-use profiles in `.conan/profiles`. `sen_gcc` and `sen_clang`
    pin the compilers CI builds with and otherwise follow the machine you run them on; `sen_msvc`
    pins Windows on `x86_64`. Use one of those three; there is no architecture to choose.

    The suffixed names exist for CI and the devcontainer, which compose one from the compiler and
    the machine's architecture and copy it into place as the default profile. Of them only
    `sen_gcc_arm` changes anything — it pins `armv8`, which is what keeps `conan.lock` from
    depending on whichever machine regenerated it. The rest add nothing to the profile they
    include. Install them with:

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

Each platform has two archives. The one ending `-release` is what you want to run. The one ending
`-relwithdebinfo` is a separate build carrying debug information, for running Sen under a debugger.
Its symbols do not describe the `-release` binaries, so a crash there has to be reproduced under it.
It is a much larger download. The installer script takes the `-release` archive unless you pass
`--debug-symbols`.

On Windows the debug information lives in `.pdb` files rather than inside the binaries. They are
installed next to the executables and DLLs in `bin`, which is where a debugger looks for them, so
keep them beside the binaries when you copy anything out of the archive.

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
    # Shared libraries and archives are under <prefix>/lib (CMAKE_INSTALL_LIBDIR). Releases
    # before that split put them in <prefix>/bin, so both are named here and whichever the
    # release you downloaded does not have costs the loader nothing.
    export LD_LIBRARY_PATH="$SEN_PREFIX/lib:$SEN_PREFIX/bin:$LD_LIBRARY_PATH"
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
release artifact), see [Building Sen from source](../howto_guides/building_from_source.md). It has
the profile to install, the component modes, the developer flags, what the build needs from the
network, and how long a first build takes.

If your editor supports devcontainers, the repository ships one under `.devcontainer/`. It builds
the same environment the pipeline uses, from `tools/ci/Dockerfile`, so you do not have to install
compilers or Conan yourself.

For enabling and running the test suite, see [Running the tests](testing.md).

## Next steps

Sen is installed. The quickest way to see it working is to generate a package, build it and run it,
which both routes below walk through.

- **[The tutorials](../tutorials/index.md)**: Tutorial 1 goes from `sen package init` to an object
  you can watch changing in the shell.
- **[Create your first package](first_package.md)**: the same ground as reference, explaining what
  `sen package init` generates and what each file is for.
