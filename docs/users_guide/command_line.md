# Command line tools

Sen comes with a command line application (called `sen`) to help you perform some actions following
the approach taken by `git`.

The best way to familiarize yourself with all the commands and the available options is to do
`--help`, starting with `sen --help`.

```shell
--8<-- "snippets/sen.sh"
```

You can also get help about the specific sub-commands.

For example:

```console title="sen command line examples"
# open a sen explorer window
sen explorer

# open a sen shell on the current terminal
sen shell

# connect to a remote sen application on localhost:8094
sen rshell localhost:8094

# starts a replay of an archive
sen replay my_archive

# runs a sen kernel from a configuration file
sen run my_config.yaml
```

## Run

```title="sen run"
--8<-- "snippets/sen_run.sh"
```

The configuration file uses YAML. Its sections, every key the kernel understands,
environment-variable substitution and the `include` merging rules are documented in
**[The configuration file](configuration.md)**, which is the reference for the format.

A short example, so this page stands on its own:

```yaml title="a minimal configuration"
load:
  - name: shell
    group: 2
    open: [ local.counters ]

build:
  - name: counterComponent
    group: 3
    freqHz: 2
    imports: [ my_counter ]
    objects:
      - name: myCounter
        class: my_counter.CounterImpl
        bus: local.counters
        step: 5
```

### Offline network footprint

Use `--print-network-footprint` to inspect the network addresses and ports that one process is
expected to use without running the application.
Sen configures and preloads the components, builds the report and exits before starting the
application.
The configuration must load the [Ether component](../components/ether.md),
which provides the report.

```console
sen run config.yaml --print-network-footprint
```

The report includes the non-local buses declared in the process configuration.
Since Sen doesn't start the application when generating the offline report, it cannot discover
buses that the application would create at runtime.
To include one of these buses, specify its `session.bus` address using `--bus <session.bus>`:

```console
sen run config.yaml --print-network-footprint --bus session.one --bus session.two
```

As an alternative, you can declare the buses in a file and pass it with `--bus-file`.
The file accepts one `session.bus` per line and `#` starts a comment.
`--bus-file` can also be repeated to add more files.

```text title="buses.txt"
# Buses
session.one
session.two  # Inline comment
```

```console
sen run config.yaml --print-network-footprint --bus-file buses.txt
```

Sen combines the buses found in the configuration with those supplied through `--bus` or
`--bus-file` and removes duplicates.

By default, Sen displays the report in a format designed to be read in the terminal.
You can select a JSON output with `--format json` option and save it:

```console
sen run config.yaml --print-network-footprint --format json > footprint.json
```

When JSON output is selected, Sen writes only the JSON document to standard output, which is the
stream redirected by `> footprint.json` in the example above.
Diagnostic and error messages are written to standard error instead, so they do not make the saved
JSON invalid.

## Code generator

`sen generate` is a thin command-line front-end over the
[`sen::gen`](gen_library.md) library, which implements the underlying
generators.

```title="sen generate"
--8<-- "snippets/sen_generate.sh"
```

### C++

```title="sen generate cpp"
--8<-- "snippets/sen_generate_cpp.sh"
```

#### C++ from STL

```title="sen generate cpp stl"
--8<-- "snippets/sen_generate_cpp_stl.sh"
```

#### C++ from HLA FOMs

```title="sen generate cpp fom"
--8<-- "snippets/sen_generate_cpp_fom.sh"
```

#### C++ exports

```title="sen generate cpp exports"
--8<-- "snippets/sen_generate_cpp_exports.sh"
```

### UML

Works the same way as C++, but with slightly different options.

```title="sen generate uml"
--8<-- "snippets/sen_generate_uml.sh"
```

#### UML from STL

```title="sen generate uml stl"
--8<-- "snippets/sen_generate_uml_stl.sh"
```

#### UML from HLA FOMs

```title="sen generate uml fom"
--8<-- "snippets/sen_generate_uml_fom.sh"
```

### MKDocs markdown

Works the same way as C++, but with slightly different options.

```title="sen generate mkdocs"
--8<-- "snippets/sen_generate_mkdocs.sh"
```

#### MKDocs from STL

```title="sen generate mkdocs stl"
--8<-- "snippets/sen_generate_mkdocs_stl.sh"
```

#### MKDocs from HLA FOMs

```title="sen generate mkdocs fom"
--8<-- "snippets/sen_generate_mkdocs_fom.sh"
```

### HTML reference

Renders the data model as a small web application: a tree, a search, and a page per type. It
needs no server and no network, so the output opens straight from a file.

```title="sen generate html"
--8<-- "snippets/sen_generate_html.sh"
```

#### HTML from STL

```title="sen generate html stl"
--8<-- "snippets/sen_generate_html_stl.sh"
```

#### HTML from HLA FOMs

```title="sen generate html fom"
--8<-- "snippets/sen_generate_html_fom.sh"
```

### Typeset reference (PDF)

Renders the data model as a Typst document, in the shape of an interface control document:
an overview, a class hierarchy, a table per type and an index. It writes Typst source rather
than a PDF, so producing one is a separate step with the [Typst](https://typst.app/) compiler.

What the document contains, and what it looks like, is set by the options rather than inferred
from the model.

```title="sen generate typst"
--8<-- "snippets/sen_generate_typst.sh"
```

#### Typst from STL

```title="sen generate typst stl"
--8<-- "snippets/sen_generate_typst_stl.sh"
```

#### Typst from HLA FOMs

```title="sen generate typst fom"
--8<-- "snippets/sen_generate_typst_fom.sh"
```

### JSON schemas

Generates json schemas from a Sen data model.

#### JSON schemas for Sen components from STL

```shell
--8<-- "snippets/sen_generate_json_component_stl.sh"
```

#### JSON schemas for Sen components from HLA FOMs

```shell
--8<-- "snippets/sen_generate_json_component_fom.sh"
```

#### JSON schemas for Sen packages from STL

```shell
--8<-- "snippets/sen_generate_json_package_stl.sh"
```

#### JSON schemas for Sen packages from HLA FOMs

```shell
--8<-- "snippets/sen_generate_json_package_fom.sh"
```

#### Combine multiple JSON schemas to create a kernel configuration schema

```shell
--8<-- "snippets/sen_generate_json_schema.sh"
```

### Python

```title="sen generate py"
--8<-- "snippets/sen_generate_py.sh"
```

#### Python from STL

```title="sen generate py stl"
--8<-- "snippets/sen_generate_py_stl.sh"
```

#### Python from HLA FOMs

```title="sen generate py fom"
--8<-- "snippets/sen_generate_py_fom.sh"
```

### TypeScript

TypeScript generation reads STL only; there is no FOM subcommand today.

```title="sen generate ts"
--8<-- "snippets/sen_generate_ts.sh"
```

#### TypeScript from STL

```title="sen generate ts stl"
--8<-- "snippets/sen_generate_ts_stl.sh"
```

## Archiving utility

Helps you interact with archives. "Archive" and "recording" are the same thing here: the recorder
writes a recording, and `sen archive` is the command that inspects one.

```title="sen archive"
--8<-- "snippets/sen_archive.sh"
```

### Basic information

```title="sen archive info"
--8<-- "snippets/sen_archive_info.sh"
```

### Indexed objects

```title="sen archive indexed"
--8<-- "snippets/sen_archive_indexed.sh"
```

## Packaging utility

```title="sen package"
--8<-- "snippets/sen_package.sh"
```

### Package skeleton creation

```title="sen package init"
--8<-- "snippets/sen_package_init.sh"
```

### Component skeleton creation

```title="sen package init-component"
--8<-- "snippets/sen_package_init-component.sh"
```

## File to array utility

```title="sen file-to-array"
--8<-- "snippets/sen_file_to_array.sh"
```
