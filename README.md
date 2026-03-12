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
- *Explorer GUI* to inspect and interact with your system (objects, events, sessions, plots).
- *REST API Server* for interfacing external (non-Sen) systems.

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

We provide binary releases and Conan packages. To use Conan:

1. Create Conan configuration file in your project's top-level directory and add **Sen** as a dependency of your project.
2. Run `conan install . --profile <your_conan_profile> --build=missing` to download Sen before running CMake.

To ensure your system is able to find all paths, add the `bin` folder to your `PATH` environment variable (in Linux also
add it to the `LD_LIBRARY_PATH`). Check that everything works by running `sen shell`. You can play with the objects in the `local` session.

To write you first package:

```shell
sen package init my_package --class MyClass               # Generate the skeleton
cd my_package                                             # Go to the new folder
cmake -S . -B build -G "Ninja" && cmake --build build     # Build it
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/build/bin  # Set the library path
sen run config.yaml                                       # Run it
```

Take a look to the examples, but there's much more to Sen, so don't forget to read the [docs](https://airbus.github.io/sen/latest/).

<a name="how-to-build"></a>
## 🔨 How to Build

You need [Conan](https://conan.io/) and a C++17 compiler (GCC, Clang, Visual Studio).

For Debian-based systems, you can get all the dependencies using:

```shell
pip install conan
sudo apt-get install -y pkg-config graphviz pytest libxext-dev
```

You can find some commonly-used conan profiles in the `.conan/profiles` folder. Those can be
installed by running `conan config install -tf profiles .conan/profiles/<profile>`. Here we use 'sen_gcc'
but you can use the profile of your choice.

```shell
conan install . --profile=sen_gcc --build=missing # Fetch third-party dependencies (only needed once)
conan build   . --profile=sen_gcc                 # Build the software
```

Alternatively, build with `cmake -S . -B build -G Ninja --preset sen_gcc && cmake --build build`.

<a name="limitations"></a>
## ⚠️ Limitations

Sen is currently under active development. If you choose to use it right now, be prepared for
potential bugs and breaking changes.

Always check the official documentation and release notes for updates and proceed with caution.
Happy coding! 🚀

<a name="contributing"></a>
## 🙌 Contributing

Contributions are encouraged and valued. Have a look at our [guidelines](CONTRIBUTING.md) for the full picture.

<a name="credits"></a>
## 💖 Credits

Huge thanks to all the people using Sen and providing active feedback!

Sen is possible thanks to the sponsorship and engagement of the Airbus engineering community.
