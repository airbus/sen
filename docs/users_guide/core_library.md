# sen::core

The Sen Core library encapsulates foundational utilities shared across all Sen components. It is
divided into the following modules:

## Base utilities (`base`)

- **Error processing** - contracts, assertions, and result types for communicating failure without
  exceptions across component boundaries.
- **General macros** - `SEN_NOCOPY_NOMOVE`, `SEN_EXPORT_CLASS`, and other helpers used throughout
  every package.
- **Compile-time template helpers** - type traits and metaprogramming utilities for building the
  generated code layer.
- **Hashing and compression** - lightweight hash functions and LZ4-based compression helpers.
- **Memory utilities** - memory pools and smart pointer wrappers.
- **General utilities** - like conversion helpers.

## Input/Output (`io`)

Serialization and deserialization utilities for the internal wire format, JSON, YAML, and XML.
Used both by the kernel and by the code generator for config parsing and introspection output.

## Meta type system (`meta`)

The runtime reflection layer:

- **Trait structures** - compile-time descriptions of every generated type (name, fields, methods,
  events, QoS attributes).
- **Type registry** - a runtime catalog of all known types, used for introspection, shell
  auto-completion, and schema validation.
- **Type manipulation utilities** - comparison, hashing, and serialization helpers that work on
  any registered type.

## STL parser and VM (`lang`)

The parser and virtual machine for the Sen Type Language (STL). The code generator invokes this
layer at build time; the kernel uses it at runtime for dynamic type loading and backward
compatibility resolution.

## Object helpers (`obj`)

Utilities for instantiating, naming, and managing the lifecycle of Sen objects. These are used by
the generated `*Base` classes that your implementations inherit from.

## See Also

- [STL language reference](stl.md) - how to define types consumed by this library
- [Generated code guide](../howto_guides/generated_code.md) - the C++ code this library underpins
- [API Reference](../doxygen_gen/html/index.html) - full Doxygen documentation (available after building the docs)
