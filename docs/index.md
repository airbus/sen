---
hide:
- navigation
- toc
---

![Screenshot](assets/images/logo.svg){: style="width:400px; display: block; margin: 0 auto"}

Sen is a simple way for applications to talk to one another to create, connect, and integrate
complex systems with ease.

Technically speaking, Sen is a general-purpose, distributed, object-oriented system with a focus on
applications that demand low-latency, high-performance, rich inter/intra process communication, high
modularity, and platform independence while providing low-overhead, full introspection and an
extensible tooling support.

Sen is the result of many years of experience in building similar solutions across different
industries in mixed-criticality production and research environments. Sen itself is born with
customers in the simulation domain, where the integration of heterogeneous systems is commonplace,
and the need for dynamic distributed solutions is always present.

If you are arriving from ROS, DDS, gRPC or SOME/IP, start with
[the mental model](users_guide/mental_model.md): it maps what you already know onto Sen's
equivalents, and explains the one idea that is genuinely different: Sen is object-oriented at the
network level, not message-oriented.

On performance: low latency and high throughput are what Sen is built for and what its execution
model is shaped around, but no figures are published yet. The benchmarking work to produce numbers
worth standing behind is in progress. Read the wording above as intent, and measure on your own
workload before designing against it.

The word "Sen" is a short (and easy to pronounce) *name* for this software. As such, it is neither
an acronym nor an abbreviation, but a noun.

## Where to start

- **New to Sen?** [Getting started](getting_started/index.md) takes you from nothing installed to
  your own package running, in two steps.
- **Want to see it work first?** [Tutorial 1: Hello Sen](tutorials/hello_sen.md) is the shortest
  path from an empty directory to a running system.
- **Prefer to read code?** The [examples](examples/index.md) are a graded set of working packages,
  ordered by complexity.
- **Want to know how it works?** The [manual](users_guide/index.md) explains the ideas, the
  interface language and the runtime.
- **Tripped up by a word?** The [glossary](users_guide/glossary.md) covers the terms Sen redefines,
  and the ones that collide with something you already know.
- **Looking for something specific?** The [how-to guides](howto_guides/index.md) each answer one
  question, and the [components](components/index.md) pages cover what ships with Sen.

## Key features

![Screenshot](assets/images/rocket_light.svg#only-light){: style="width:350px; float: right;"}
![Screenshot](assets/images/rocket_dark.svg#only-dark){: style="width:350px; float: right;"}

### Architecture

- Distributed component-based system for easy microservice-based solutions.
- Object-oriented and event-driven architecture on top of a light (user-space) micro-kernel.
- Package-based, plugin-oriented system for higher reuse, modularity and lower coupling.
- Rich type system[^1] with full compile-time and run-time introspection.
- Generates your types from HLA FOM files, so a SISO standard model such as RPR or NETN can
  serve as your interface definition. Sen reads FOMs at build time; it does not join HLA
  federations; see [Using HLA FOMs](users_guide/hla.md).
- Simple language for easy definition of your interfaces (Sen Type Language).

### Execution model

- Real-time, faster-than real-time (as fast as possible) and stepped execution.
- Built-in ownership of objects and their state: each object belongs to the component that
  created it, and goes away when that component does. This is lifetime ownership, not
  transfer between components.
- Inherently asynchronous system. Callers cannot be blocked. Callees can postpone their execution.
- Thread-safe: your components don't need to use synchronization primitives. Code that Sen calls
  runs inside the cycle; a thread you started yourself is outside it and hands work in through the
  `preDrain()` and `preCommit()` hooks rather than writing directly; see
  [Connecting existing systems](howto_guides/connecting_existing_systems.md).
- Dependency management and controlled component execution by groups.
- Built-in type-safe configuration mechanism based on YAML[^2].

### Communication model

- Conditional subscription with both producer-side and consumer-side filtering[^3].
- Data segregation enabled through the usage of dedicated logical buses.
- Broker-less design. Nothing relays traffic; participants discover each other over multicast, or
  through a discovery hub where multicast is not available.
- Quality-of-service attributes: confirmed & ordered, best-effort directed, best-effort broadcast.
- Generation of documentation, web pages, UML diagrams, and other formats out of the ICD definition.
- Pluggable transport design. The kernel talks to a transport through an interface, so a
  deployment can carry Sen traffic over something other than `ether`.

### Shipped components

- *Ethernet transport* that supports asynchronous I/O over TCP, UDP unicast and multicast.
- *Recorder*, highly customizable, with LZ4 compression, indexes, snapshots, annotations, etc.
- *Replayer* with support for real-time, stepped execution and random access.
- *Python Interpreter* that can be instantiated. You can script your components and tests.
- *Shell* for CLI interaction, with auto-completion, introspection, and remote connectivity.
- *Grafana visualization* via the InfluxDB component.
- *Explorer GUI* to inspect and interact with your system (objects, events, sessions, plots).
- *Web Explorer* serving the same thing from Sen itself, to a browser.
- *JSON-RPC* 2.0 over WebSocket, with a TypeScript client for browser and Node applications.
- *MCP gateway* so a large language model can observe and drive a running system, or read a
  recording.
- *Log Manager* to have full control over your logs.
- *Tracer* to use the frame-based tracer [Tracy](https://github.com/wolfpld/tracy) to inspect the
  behavior of your processes.
- *REST* API to interface with Sen through HTTP endpoints.

### Implementation

- Lightweight, multi-platform implementation. Works on Linux and Windows.
- Built-in serialization engine.
- Run-time and compile-time introspection provided by the code generator.
- Optimized memory management by extensive use of memory pools.
- Natively integrated with spdlog.
- Natively integrated with CMake. Compiler and version info is baked into the binaries.
- Self-contained: no 3rd-party dependencies on the public interface.
- Code generation template engine that allows extending and enhancing it to our future needs.
- Container-friendly.
- Python bindings for accessing recorded data.
- Backward compatible ICDs with runtime interoperability. Where two participants disagree about a
  type, Sen adapts rather than refuses, as far as the types allow (see
  [Compatibility conversions](users_guide/compatibility_conversions.md) for details).
- Configuration is defined using YAML or Python.

## Motivation

Technically, most innovative solutions aren't new; it is how we use and integrate them in clever and
unique ways that makes them facilitators for our business. Infrastructures enable this integration,
and by simplifying their use, engineers adopt them, migrating products step-by-step into the next
generations. Sen is a software infrastructure designed to be fast, efficient, and lightweight,
serving as a base for user applications. Built on substantial prior experience, Sen is tailored for
(but not limited to) simulation-related applications.

Sen's high-level objectives are:

- **Simplify system integration**: As a communications backbone, Sen helps to integrate systems,
  reducing the need for monoliths and minimizing differences between system components. This
  enhances testability, separability, and interoperability with third-party solutions.
- **Reduce complexity**: Sen controls complexity by defining a way of working and organizing
  knowledge into independent, reusable, and interconnected parts.
- **Avoid duplication**: Beyond IPC, Sen connects ideas, knowledge, tools, data, and solutions. It
  creates a shared repository of interchangeable functional components.

## Open source

**Sen** is Free and Open Source under the permissive Apache 2.0 license. Browse the source, ask
questions, report bugs, or suggest improvements.

Sen is built and maintained by the people named in
[AUTHORS.md](https://github.com/airbus/sen/blob/main/AUTHORS.md). Issues and pull requests go
through [the repository](https://github.com/airbus/sen), and
[CONTRIBUTING.md](https://github.com/airbus/sen/blob/main/CONTRIBUTING.md) covers how to build,
test and submit a change.

[^1]: Apart from all basic types, it supports classes, structures, inheritance, enumerations,
    bounded and unbounded sequences, arrays, optionals, variants, quantities (with support for units
    incl. conversion and ranges), etc.

[^2]: You define the configuration values in YAML, and automatically get your data in your native
    language's strongly-typed representation.

[^3]: Using a native query language that supports arbitrarily complex conditions. Data is only sent
    if needed, when needed, and only to those who need it.
