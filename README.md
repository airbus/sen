![logo](docs/assets/images/logo_readme.svg)

*General-purpose, distributed, object-oriented system for applications that demand high modularity
and rich communication.*

[![Build documentation](https://github.com/airbus/sen/actions/workflows/build_docs.yaml/badge.svg)](https://github.com/airbus/sen/actions/workflows/build_docs.yaml)

![----------------------------------------------](docs/assets/images/separator1.png)

## 📖 Table of Contents

![----------------------------------------------](docs/assets/images/separator1.png)

- [Overview ➤](#-overview)
- [License ➤](#-license)
- [How to Build ➤](#-how-to-build)
- [Quick start ➤](#-quick-start)
- [Main Features ➤](#-main-features)
- [Limitations ➤](#-limitations)
- [Contributing ➤](#-contributing)
- [Acknowledgements ➤](#-acknowledgements)

## ☁️ Overview

![----------------------------------------------](docs/assets/images/separator1.png)

Sen is a simple way for applications to talk to one another and create, connect, and integrate
complex systems with ease.

Technically speaking:

Sen is a general-purpose, distributed, object-oriented system with a focus on applications that
demand low-latency, high-performance, rich inter/intra process communication, high modularity, and
platform independence while providing low-overhead, full introspection and an extensible tooling
support.

## 📄 License

![----------------------------------------------](docs/assets/images/separator1.png)

This project is licensed under the terms of the
[Apache 2.0 License](https://www.apache.org/licenses/LICENSE-2.0).

## 🔨 How to Build

You need Conan and a C++17 compiler.

For Debian-based systems, you can get all the mandatory and optional system dependencies using:

```shell
pip install conan
sudo apt-get install -y build-essential gdb ninja-build pkg-config cmake doxygen graphviz libgl1-mesa-dev libxext-dev
```

You can find some commonly-used conan profiles in the `.conan/profiles` folder. Those can be
installed by running `conan config install -tf profiles .conan/profiles/<profile>`.

Then, use Conan to fetch the third-party dependencies `conan install . --profile=sen_gcc --build=missing`
(you can replace 'sen_gcc' with the preset of your choice).

To build, use `conan build . --profile=sen_gcc`. Alternatively, use
`cmake -S . -B build -G Ninja --preset sen_gcc && cmake --build build` (you can replace 'sen_gcc'
with the preset of your choice).

## ⚡ Quick start

![----------------------------------------------](docs/assets/images/separator1.png)

Sen releases are hosted on Conan-Center. To get it:

1. Create Conan configuration file in your project's top-level directory and add **Sen** as a dependency of your
   project.
2. Run `conan build . --profile <your_conan_profile> --build=missing` to download Sen before running CMake.

We also provide binary releases in zip packages that you can download. To ensure your system is able to find all
the paths, add the `bin` folder to your `PATH` environment variable. In the case of Linux, also add it to the
`LD_LIBRARY_PATH` and `DYLD_LIBRARY_PATH`.

Check that everything works by running `sen shell`. You can play with the objects in the local bus.

To write you first package:

```shell
sen package init my_package --class MyClass               # Generate the skeleton
cd my_package                                             # Go to the new folder
cmake -S . -B build -G "Ninja" && cmake --build build     # Build it
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/build/bin  # Set the library path
sen run config.yaml                                       # Run it
```

Take a look to the examples, but there's much more to Sen, so don't forget to read the docs.

## 🌟 Main Features

![----------------------------------------------](docs/assets/images/separator1.png)

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
  [LZ4](<https://en.wikipedia.org/wiki/LZ4_(compression_algorithm)>) compression, indexes, snapshots
  and annotations.

- *Ethernet transport* supporting asynchronous I/O over TCP, UDP unicast and multicast.

- *Replayer* with support for real-time, stepped execution and random access.

- *Python Interpreter* that can be instantiated. You can script your components and tests.

- *Shell* for CLI interaction, with auto-completion, introspection, and remote connectivity.

- *[Grafana](https://grafana.com/) visualization* via the [InfluxDB](https://www.influxdata.com/)
  component.

- *Explorer GUI* to inspect and interact with your system (objects, events, sessions, plots).

- *REST API Server* for interfacing external (non-Sen) systems.

**💻 Implementation**

- Lightweight, multi-platform implementation (C++17). Works on Linux and Windows.

- Run-time and compile-time introspection provided by the code generator.

- Optimized memory management by extensive use of memory pools.

- Natively integrated with [CMake](https://cmake.org/). Meta info is baked into the binaries.

- Self-contained: no 3rd-party dependencies on the public interface.

- Python bindings for accessing recorded data.

- Backward compatible ICDs with runtime interoperability.

## ⚠️ Limitations

![----------------------------------------------](docs/assets/images/separator1.png)

Sen is currently under active development. If you choose to use it right now, be prepared for
potential bugs and breaking changes.

Always check the official documentation and release notes for updates and proceed with caution.
Happy coding! 🚀

## 🙌 Contributing

![----------------------------------------------](docs/assets/images/separator1.png)

Contributions are encouraged and valued. Have a look at our
[contributing guidelines](.github/CONTRIBUTING.md) for the full picture.

## ❤️ Acknowledgements

![----------------------------------------------](docs/assets/images/separator1.png)

Huge thanks to all the people using Sen and providing active feedback!

Sen is possible thanks to the sponsorship and engagement of the Airbus engineering community.

______________________________________________________________________
