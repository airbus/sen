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

The word "Sen" is a short (and easy to pronounce) *name* for this software. As such, it is neither
an acronym nor an abbreviation, but a noun.

## Key Features

![Screenshot](assets/images/rocket_light.svg#only-light){: style="width:350px; float: right;"}
![Screenshot](assets/images/rocket_dark.svg#only-dark){: style="width:350px; float: right;"}

### Architecture

- Distributed component-based system for easy microservice-based solutions.
- Object-oriented and event-driven architecture on top of a light (user-space) micro-kernel.
- Package-based, plugin-oriented system for higher reuse, modularity and lower coupling.
- Rich type system[^1] with full compile-time and run-time introspection.
- Native support of HLA FOMs. You can directly use the SISO standards as your ICD.
- Simple language for easy definition of your interfaces (Sen Type Language).

### Execution Model

- Real-time, faster-than real-time (as fast as possible) and stepped execution.
- Built-in object and data ownership management.
- Inherently asynchronous system. Callers cannot be blocked. Callees can postpone their execution.
- Thread-safe: your components don't need to use synchronization primitives.
- Dependency management and controlled component execution by levels and groups.
- Built-in type-safe configuration mechanism based on YAML[^2].

### Communication Model

- Conditional subscription with both producer-side and consumer-side filtering[^3].
- Data segregation enabled through the usage of dedicated logical buses.
- Broker-less design. No central orchestrator required. Participants discover each-other.
- Quality-of-service attributes: confirmed & ordered, best-effort directed, best-effort broadcast.
- Generation of documentation, web pages, UML diagrams, and other formats out of the ICD definition.
- Pluggable transport design. Allows multiple data transport solutions.

### Shipped components

- *Ethernet transport* that supports asynchronous I/O over TCP, UDP unicast and multicast.
- *Recorder*, highly customizable, with LZ4 compression, indexes, snapshots, annotations, etc.
- *Replayer* with support for real-time, stepped execution and random access.
- *Python Interpreter* that can be instantiated. You can script your components and tests.
- *Shell* for CLI interaction, with auto-completion, introspection, and remote connectivity.
- *Grafana visualization* via the InfluxDB component.
- *Explorer GUI* to inspect and interact with your system (objects, events, sessions, plots).
- *Log Manager* to have full control over your logs.
- *Tracer* to use the frame-based tracer [Tracy](https://github.com/wolfpld/tracy) to inspect the
  behaviour of your processes.
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
- Backward compatible ICDs with runtime interoperability.
- Configuration is defined using YAML or Python.

## Motivation

Technically, most innovative solutions aren't new; It is how we use and integrate them in clever and
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

## Open Source

**Sen** is Free and Open Source under the permissive Apache 2.0 license. Browse the source, ask
questions, report bugs, or suggest improvements.

[^1]: Apart from all basic types, it supports classes, structures, inheritance, enumerations, bounded and
    unbounded sequences, unions, variants, dictionaries, quantities (with support for units incl.
    conversion and ranges), etc.

[^2]: You define the configuration values in YAML, and automatically get your data in your native
    language's strongly-typed representation.

[^3]: Using a native query language that supports arbitrarily complex conditions. Data is only sent if
    needed, when needed, and only to those who need it.
