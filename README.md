<h1 align="center"> <img src="docs/assets/images/logo_readme.svg" alt="Sen" width="400"> </h1>

<h4 align="center">General-purpose, distributed, object-oriented system for applications that demand high modularity
and rich communication.</h4>

<div align="center">
  <a href="#overview">Overview</a> •
  <a href="#main-features">Main Features</a> •
  <a href="#quick-start">Quick Start</a> •
  <a href="#how-to-build">How to Build</a> •
  <a href="#limitations">Limitations</a> •
  <a href="#contributing">Contributing</a> •
  <a href="#credits">Credits</a> •
  <a href="https://airbus.github.io/sen/latest/">Docs</a>
</div>

---

<div align="center">

<a href="">![License](https://img.shields.io/badge/License-Apache%202.0-blue)</a>
<a href="">![C++](https://img.shields.io/badge/C%2B%2B-17-blue?logo=c%2B%2B)</a>
<a href="">[![Documentation](https://github.com/airbus/sen/actions/workflows/build_docs.yaml/badge.svg)](https://github.com/airbus/sen/actions/workflows/build_docs.yaml)</a>

</div>

<a name="overview"></a>

## 🚀 Overview

Sen is a simple way for applications to talk to one another and create, connect, and integrate
complex systems with ease.

Technically speaking, Sen is a general-purpose, distributed, object-oriented system with a focus on applications that
demand low-latency, high-performance, rich inter/intra process communication, high modularity, and platform
independence while providing low-overhead, full introspection and an extensible tooling support.

<a name="main-features"></a>

## Main Features

**🏗️ Architecture**

- Distributed component-based system for easy microservice-based solutions.
- Object-oriented and event-driven architecture on top of a light (user-space) micro-kernel.
- Package-based, plugin-oriented system for higher reuse, modularity and lower coupling.
- Rich type system with full compile-time and run-time introspection.
- Native support of [HLA FOMs](https://en.wikipedia.org/wiki/High_Level_Architecture). You can
  directly use the [SISO](https://www.sisostandards.org/) standards as your
  [ICD](https://en.wikipedia.org/wiki/Interface_control_document).
- Simple language for easy definition of your interfaces: Sen Type Language (STL).

**⚙️ Execution model**

- Real-time, faster-than real-time (as fast as possible) and stepped execution.
- Built-in object and data ownership management.
- Inherently asynchronous. Callers cannot be blocked. Callees can postpone their execution.
- Thread-safe: your components don't need to use synchronization primitives.
- Dependency management and controlled component execution by levels and groups.
- Built-in type-safe configuration mechanism based on [YAML](https://yaml.org/) or
  [Python](https://www.python.org/).

**🔗 Communications model**

- Conditional subscription with both producer-side and consumer-side filtering.
- Data segregation enabled through the usage of dedicated logical buses.
- [Broker-less](https://en.wikipedia.org/wiki/Message_broker) design. No central orchestrator
  required. Participants discover each-other.
- Quality-of-service attributes: confirmed & ordered, best-effort directed, best-effort broadcast.
- Generation of documentation and [UML](https://en.wikipedia.org/wiki/Unified_Modeling_Language)
  diagrams and [MkDocs](https://www.mkdocs.org/) out of the ICD definition.
- Pluggable data transport, allowing multiple implementations.

**📦 Shipped components**

- *Recorder*, highly customizable, with
  [LZ4](<https://en.wikipedia.org/wiki/LZ4_(compression_algorithm)>) compression, indexes, snapshots and annotations.
- *Ethernet transport* supporting asynchronous I/O over TCP, UDP unicast and multicast.
- *Replayer* with support for real-time, stepped execution and random access.
- *Python Interpreter* embedded. You can script your components and tests.
- *Shell* for CLI interaction, with auto-completion, introspection, and remote connectivity.
- *[Grafana](https://grafana.com/) visualization* via the [InfluxDB](https://www.influxdata.com/) component.
- *Tracer* based on the excellent [Tracy](https://github.com/wolfpld/tracy) frame-based profiler.
- *Explorer GUI* to inspect and interact with your system (objects, events, sessions, plots),
  available as either a native desktop window or a browser-based Web Explorer.
- *REST API Server* or *JSON-RPC over WebSocket* for interfacing external (non-Sen) systems,
  with an in-tree TypeScript client (`@sen/client`) for browser / Node.js consumers.

**💻 Implementation**

- Lightweight, multi-platform implementation. Works on Linux and Windows.
- Run-time and compile-time introspection provided by the code generator.
- Optimized memory management by extensive use of memory pools.
- Natively integrated with [CMake](https://cmake.org/). Meta info is baked into the binaries.
- Self-contained: no 3rd-party dependencies on the public interface.
- Python bindings for accessing recorded data.
- Backward compatible ICDs with runtime interoperability.

<a name="quick-start"></a>

## ⚡ Quick start

The fastest way to try Sen on Linux: no Conan setup required:

```shell
curl -sSf https://raw.githubusercontent.com/airbus/sen/main/resources/installer/install.sh | sh
```

The installer downloads a release into `~/.sen/<build-id>/` and writes activate scripts you source from your shell.
See the [install guide](https://airbus.github.io/sen/latest/getting_started/install/) for details.

To use Sen as a Conan dependency in your own project:

1. Create a `conanfile.py` in your project's top-level directory and add **Sen** as a dependency:

```python
from conan import ConanFile

class MyProjectConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeDeps", "CMakeToolchain", "VirtualRunEnv"

    def requirements(self):
        self.requires("sen/[>=1.0]")
```

1. Run `conan install . --profile <your_conan_profile> --build=missing` to download Sen before running CMake.

To ensure your system is able to find all paths, add the `bin` folder to your `PATH` environment variable (in Linux also
add it to the `LD_LIBRARY_PATH`). Check that everything works by running `sen shell`. You can play with the objects in
the `local` session.

To write your first package:

```shell
sen package init my_package --class MyClass               # Generate the skeleton
cd my_package                                             # Go to the new folder
cmake -S . -B build -G "Ninja" && cmake --build build     # Build it
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/build/bin  # Set the library path
sen run config.yaml                                       # Run it
```

`config.yaml` is the run configuration generated by `sen package init`. It declares which packages to load, which
objects to instantiate, and on which bus they communicate:

```yaml
load:
  - name: shell                   # load the shell component
    group: 2                      # start it in group 2
    open: [mySession.myBus]       # automatically open this bus in the shell

build:
  - name: myComponent                  # build this component
    freqHz: 30                        # run it at 30 Hz
    imports: [my_package]             # load our package
    group: 3                          # run it after the shell
    objects:
      - class: my_package.MyClassImpl # instantiate this class
        name: myObject                # set the name of the object
        bus: mySession.myBus          # publish this object to the bus
```

Take a look at the examples, but there's much more to Sen, so don't forget to read the [docs](https://airbus.github.io/sen/latest/).

<a name="how-to-build"></a>

## 🔨 How to Build

You need [Conan](https://conan.io/), a C++17 compiler (GCC, Clang, Visual Studio), CMake and
pkg-config; the last two are used by the third-party recipes when they build from source.
On Debian/Ubuntu:

```shell
sudo apt install build-essential g++-12 cmake pkg-config python3-pip
pip install conan
conan profile detect  # Once per machine: creates conan's default build profile
```

The `.conan/profiles` folder holds the profiles this project builds with. Install the whole
folder, because the per-architecture profiles include a shared base:

```shell
conan config install -tf profiles .conan/profiles/
```

Then build with the profile that matches your machine: `sen_gcc_x86` on Intel and AMD,
`sen_gcc_arm` on arm hardware such as Apple Silicon. Both pin `gcc-12`, which has to exist on
your machine. Picking the wrong one fails while installing system libraries, because Conan
then looks for packages built for the other architecture.

```shell
conan install . --profile=sen_gcc_x86 --build=missing \
    -c tools.system.package_manager:mode=install \
    -c tools.system.package_manager:sudo=True    # Fetch third-party dependencies (only needed once)
conan build   . --profile=sen_gcc_x86            # Build Sen
```

The package manager conf lets recipes install the system libraries they need (drop the sudo
line when you already run as root, for example in a container). The first install compiles
every third-party dependency and takes a while; later builds reuse them.

If you use an editor with devcontainer support, the repository ships one under
`.devcontainer/`: open the folder in a container and the compilers, Conan and the test tools are
already installed. CMake, Ninja and Node arrive with the first `conan install`, as they do on any
other machine.

To also build the examples, pass `-o "sen/*:with_examples=True"` to `conan install`, and see
[examples/README.md](examples/README.md) for running them.

Opening Sen in an editor: `conan install` writes `CMakeUserPresets.json` at the repository root,
so VS Code, CLion and Visual Studio list the generated preset once you open the folder. See
[CONTRIBUTING.md](CONTRIBUTING.md) for the details.

Alternatively, if you want to drive CMake yourself:

```shell
conan install . --profile=sen_gcc_x86 --build=missing # Fetch third-party dependencies (only needed once)
source build/gcc/Release/generators/conanbuild.sh # Make conan's tools (cmake, ninja) available
cmake --preset conan-gcc-release                  # Generate the build system
cmake --build --preset conan-gcc-release          # Build Sen
```

If you would like to set up the full development environment for Sen (incl. testing, docs, etc...),
you would need to install the `pytest`, `graphviz` and `plantuml` packages using your package manager.

Build configuration is driven by a coarse `mode` Conan option
(`barebones`/`basic`/`full`) that selects which components compile and which deps Conan
fetches. Per-component opt-out happens at the CMake step via `-DSEN_BUILD_<NAME>=OFF`.
Developer-facing flags (`with_examples`, `with_tests`, `with_clang_tidy`, `with_coverage`,
`with_docs`, `sanitizer`) toggle the matching CMake flags. See the
[Building Sen](docs/getting_started/install.md#build-options) page for examples.

The first full-mode build fetches its toolchain (including Node.js for the browser UI) and all
third-party packages from Conan Center and the npm registry — details and opt-outs in
[what the build needs](docs/getting_started/install.md). To run the test suite, see
[Running the Tests](docs/getting_started/testing.md); for a quick tour of a running system,
try the [Web Explorer showcase](docs/components/webexplorer.md#try-it-standalone).

<a name="local-workspace-setup"></a>

## 📦 Local Workspace Setup (Conan Editable Mode)

You can link consumer projects that have Sen as a dependency with a local compilation of Sen
without running `conan create` by using conan editable mode. Just follow this steps:

1. Configure the current Sen version in the editable mode list (this version needs to match the one
   specified in the requirements section of the consumer project)
   ```bash
   conan_channel=$([ -n "$TAG_NAME" ] && echo "stable" || echo "devel")
   conan editable add . --user=airbus --channel=$conan_channel
   ```
   You can also run `conan editable list` to check if the package was correctly added to the list.
2. Run conan install in the consumer project and check that the Sen package used as dependency is
   marked as `Editable`. In that case, the consumer package will be compiled with the local Sen.
3. To remove Sen from the conan editable list, just run:
     ```bash
   conan editable remove .
   ```

<a name="limitations"></a>

## ⚠️ Limitations

Sen is under active development. Expect potential bugs and breaking changes between releases.

- The public API is not yet stable: check the release notes before upgrading.
- Some features may be undocumented or partially implemented.
- Windows support is available but less battle-tested than Linux.

Open an [issue](https://github.com/airbus/sen/issues) if you hit a problem, and always consult the
[docs](https://airbus.github.io/sen/latest/) for the latest guidance.

<a name="contributing"></a>

## 🙌 Contributing

Contributions are encouraged and valued. Have a look at our [guidelines](CONTRIBUTING.md) for the full picture.

<a name="credits"></a>

## 💖 Credits

Huge thanks to all the people using Sen and providing active feedback!

Sen is possible thanks to the sponsorship and engagement of the Airbus engineering community.
