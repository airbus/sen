# sen::core

The Sen Core library encapsulates foundational utilities shared across all Sen components. It is
divided into the following modules:

## Base utilities (`base`)

- **Error processing**: contracts, assertions, and result types for communicating failure without
  exceptions across component boundaries.
- **General macros**: `SEN_NOCOPY_NOMOVE` and the other compiler helpers used throughout every
  package.
- **Compile-time template helpers**: type traits and metaprogramming utilities for building the
  generated code layer.
- **Hashing and compression**: lightweight hash functions and compression helpers.
- **Memory utilities**: memory pools and smart pointer wrappers.
- **General utilities**: like conversion helpers.

## Input/Output (`io`)

Reading and writing the internal binary wire format: the streams that encode every basic type, and
the helpers that adapt a value to its type before encoding. The buses and the recorder both write
this format.

This module does not handle JSON, YAML or XML. Inside this library, the `meta` module converts a
`Var` to and from those interchange formats, and the `lang` module parses HLA FOM XML. Outside it,
the kernel parses the YAML configuration files, the `jsonrpc` component encodes JSON for its
clients, and `sen generate json` writes the schema and package descriptions. What `io` contributes
is `adaptVariant`, which rewrites variant tags from indices into type names so that an encoder can
name them.

## Meta type system (`meta`)

The runtime reflection layer:

- **Trait structures**: compile-time descriptions of every generated type (name, fields, methods,
  events, QoS attributes).
- **Type registry**: a runtime catalog of all known types, used for introspection, shell
  auto-completion, and schema validation.
- **Type manipulation utilities**: comparison, hashing, and serialization helpers that work on
  any registered type.
- **Class registration**: `SEN_EXPORT_CLASS`, which gives a generated class the two entry points
  the kernel needs to describe it and to instantiate it by name.
- **Interchange conversions**: `toJson`, `toCbor`, `toMsgpack`, `toUbjson`, `toBson` and their
  inverses, converting any `Var` to and from those formats.

## STL parser and VM (`lang`)

The parser and virtual machine for the Sen Type Language (STL). The code generator invokes this
layer at build time; the kernel uses it at runtime for dynamic type loading and backward
compatibility resolution.

## Object helpers (`obj`)

Utilities for instantiating, naming, and managing the lifecycle of Sen objects. These are used by
the generated `*Base` classes that your implementations inherit from.

## See Also

- [STL language reference](stl.md): how to define types consumed by this library
- [Generated code guide](../howto_guides/generated_code.md): the C++ code this library underpins
- [API Reference](../doxygen_gen/html/index.html): full Doxygen documentation (available after
  building the docs)
