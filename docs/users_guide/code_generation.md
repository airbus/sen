# Code generation

You write your interface once, in the [Sen Type Language](stl.md) or as an
[HLA FOM](hla.md), and Sen writes things from it: the C++ you program against, a schema that checks
your configuration, a reference somebody else can read.

## What you can generate

The names below are the ones you type after `sen generate`.

| | What you get | More |
| --- | --- | --- |
| `cpp` | The C++ your implementation derives from, and the types it uses. | [The generated C++](../howto_guides/generated_code.md) |
| `py` | Python dataclasses holding an object's name and its properties, so a script can build your configuration instead of you typing YAML. | [Python as config](../howto_guides/python_config.md) |
| `json` | A JSON schema describing what your package or component contributes to a configuration file. | [Getting your editor to check the file](configuration.md#getting-your-editor-to-check-the-file) |
| `ts` | TypeScript declarations for your data types, and an interface for each event's payload. | [Command line tools](command_line.md#typescript) |
| `uml` | A PlantUML document describing the model. | [UML generation](../examples/generated_uml.md) |
| `mkdocs` | One markdown file covering every type, for a Material-flavoured MkDocs site. | [Command line tools](command_line.md#mkdocs-markdown) |
| `html` | A browsable reference that opens from a file: a tree, a search, and a view per type showing what it carries and what uses it. | [HTML reference](../examples/generated_reference.md) |

## Getting it as part of your build

If you build a package with `add_sen_package`, the C++ is generated for you and so is the package's
JSON schema. Most projects need nothing else.

For a diagram or a reference, add a target and build it when you want the output. These targets are
not part of the default build, so nothing slows down until you ask:

```cmake
find_package(sen REQUIRED)

sen_generate_html(
  TARGET model_reference
  OUT ${CMAKE_CURRENT_BINARY_DIR}/reference
  STL_FILES stl/my_package/my_class.stl
)
```

```shell
cmake --build <your build directory> --target model_reference
```

Pass one of `STL_FILES` or `HLA_FOM_DIRS`. With neither, the call is accepted and no target is
created, so the build reports an unknown target rather than a mistake in your `CMakeLists.txt`.

To have the output built every time instead, depend on it from something that is in the default
build:

```cmake
add_dependencies(my_library model_reference)
```

If your package uses `TEST_TARGET`, that dependency belongs on `<target>_obj` rather than on the
target itself, for the reason [CMake integration](cmake.md) gives.

To generate into a target you made yourself, `sen_generate_cpp` and `sen_generate_python` add their
output to a target that already exists. That target has to be a compilable C++ one: both call Sen's
target setup, which requires C++17 of anything that links it, turns on warnings as errors and links
`sen::core`. An interface library or a custom target is rejected.

There is no CMake function for `mkdocs` or for `ts`, which does not mean you cannot generate them
from a build: run `sen::cli_gen` from an `add_custom_command` instead, as Sen's own TypeScript
client does. You have to name the output files yourself, because CMake cannot know in advance what
the generator will write.

[CMake integration](cmake.md) documents the
functions that do exist, including `sen_generate_yaml` for running a Python script that writes
configuration, and `sen_combine_schemas` for merging schemas into the one an editor reads.

## Getting it from the command line

`sen generate` reaches every generator. Each takes STL files as positional arguments, or FOM
directories behind `--directories`:

```shell
sen generate html stl stl/my_package/my_class.stl -i stl --output reference
sen generate html fom --directories rpr netn --output reference
```

`-i` is where imported STL files are looked up. Output options differ: most generators take
`--output`, `ts` takes `--out-dir`, and `cpp` and `py` write into the current directory.

`ts` reads STL only. `json` takes one more word, because it writes more than one kind of schema:
`sen generate json package stl`, or `component` for a component's own configuration. Two
subcommands take no model from you at all: `sen generate json schema` merges schemas you already
have, and `sen generate cpp exports` writes a package's export file.

[Command line tools](command_line.md#code-generator) has the options for each.

## Things worth knowing before you rely on them

**`uml` writes text, not a picture.** The output is a PlantUML document. Turning it into an image
needs the `plantuml` program, which needs a Java runtime. By default the document covers structs,
enums and variants as well as classes; `CLASSES_ONLY` in CMake, or `--only-classes` on the command
line, narrows it to the class diagram most people picture.

**`py` does not replace YAML.** Sen reads YAML. The dataclasses are there so a Python script can
build the configuration and write the YAML out.

**`ts` types your data, not your calls.** Structs, enums and variants come out as TypeScript
declarations, and each event gets an interface for its payload. Methods and properties are not
generated, so calls into an object are still made by name.

**`mkdocs` writes one file, and expects Material.** Everything lands in a single markdown document,
and it uses content tabs and icon shortcodes that need
[Material for MkDocs](https://squidfunk.github.io/mkdocs-material/) rather than plain MkDocs.

The generators are also a library, [`sen::gen`](gen_library.md), for a program that has to write any
of this while it runs rather than while it builds.
