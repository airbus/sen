# sen::gen

This library bundles the code generators used to translate Sen type sets (`.stl`
files, parsed into `sen::lang::TypeSet`) into source artefacts in various target
languages and documentation formats.

The library is CLI-free: its public headers depend only on Sen types and the
standard library, which lets it be linked from any Sen binary that needs to
render schemas or sources at runtime (for example, the JSON-RPC wire ships
JSON-Schema descriptions alongside custom type metadata).

The [`sen generate`](command_line.md#code-generator) command-line tool is the
canonical front-end; it is a thin shell around the generators in this library.

Check out the [API Reference](../doxygen_gen/html/group__gen.html) for the
complete list of generators and their options.

## What it contains

Each generator is exposed as a small PImpl class. Most own an inja template
environment; the HTML one builds its output directly, because it emits an
application and a data file rather than rendered text. Instances are reusable
across calls; prefer constructing one per process and feeding it successive type
sets.

| Header | Class | Output |
| --- | --- | --- |
| `sen/gen/cpp.h` | `sen::gen::CppGenerator` | C++ headers and sources |
| `sen/gen/python.h` | `sen::gen::PythonGenerator` | Python dataclasses |
| `sen/gen/typescript.h` | `sen::gen::TypeScriptGenerator` | TypeScript modules + barrel |
| `sen/gen/json.h` | `sen::gen::JsonGenerator` | JSON-Schema documents |
| `sen/gen/plantuml.h` | `sen::gen::PlantUMLGenerator` | PlantUML class diagrams |
| `sen/gen/mkdocs.h` | `sen::gen::MkDocsGenerator` | MkDocs-flavoured markdown |
| `sen/gen/html.h` | `sen::gen::HtmlGenerator` | Browsable HTML reference |

## How to use it

Parse the input STL files into a `sen::lang::TypeSetContext` (see
`sen::core`), construct the relevant generator, and call its `generate*`
method. Most return the rendered file contents as a `std::string`. The ones that
write more than one file — C++, TypeScript and HTML — return a
`FileContents`, a `std::map<path, string>` keyed by the path each file goes to.

For a fully worked example, see the `apps/cli_gen` sources: each per-topic
subcommand under `apps/cli_gen/src/*_cli.cpp` instantiates one generator and
writes its output to disk.
