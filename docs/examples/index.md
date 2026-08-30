# Examples

## Suggested learning path

Work through these in order. Each step builds on the previous one.

### 1. Understand the concepts

Read [Mental model](../users_guide/mental_model.md) and [Main
concepts](../users_guide/main_concepts.md).

### 2. Create your first package

Follow [Create your first package](../getting_started/first_package.md) to generate a skeleton,
compile it, and run it under a Sen kernel.

### 3. Work through the examples

The table is ordered by complexity, so working down it is a reasonable path. The numbers are the
directory names rather than a sequence, so there are gaps where an example was retired:

| #   | Example                                                      | What you learn                                             |
| --- | ------------------------------------------------------------ | ---------------------------------------------------------- |
| 0   | [Counter](../snippets/examples/config/0_counter/readme.md)         | The introductory package built in Getting Started          |
| 1   | [Calculators](../snippets/examples/config/1_calculators/readme.md) | Basic package, multiple implementations, shell interaction |
| 2   | [Inheritance](../snippets/examples/config/2_inheritance/readme.md) | STL inheritance, template injection                        |
| 3   | [Aircraft](../snippets/examples/config/3_aircraft/readme.md)       | HLA FOMs, `update()` loop, virtual time                    |
| 4   | [School](../snippets/examples/config/4_school/readme.md)           | Object discovery, events, multi-component                  |
| 6   | [Recorder](../snippets/examples/config/6_recorder/readme.md)       | Recording, Python post-processing                          |
| 7   | [Replayer](../snippets/examples/config/7_replayer/readme.md)       | Replay with real-time and stepped execution                |
| 8   | [InfluxDB](../snippets/examples/config/8_influx/readme.md)         | Grafana visualisation                                      |
| 9   | [Request/response servers](../snippets/examples/config/9_hla_servers/readme.md) | Method return values, async callbacks, live query objects  |
| 10  | [Python](../snippets/examples/config/10_python/readme.md)          | Embedded Python scripting                                  |
| 11  | [Shapes](../snippets/examples/config/11_shapes/readme.md)          | Interest management, Sen Query Language                    |
| 12  | [Fibonacci](../snippets/examples/config/12_fibonacci/readme.md)    | Deferred methods, load balancing                           |
| 13  | [Timer](../snippets/examples/config/13_timer/readme.md)            | Checked properties, state validation                       |
| 14  | [JSON-RPC](../snippets/examples/config/14_jsonrpc/readme.md)       | Exposing a package over JSON-RPC on a WebSocket            |

### 4. Go deeper with the how-to guides

Once you are comfortable with the examples, the [How-to guides](../howto_guides/objects.md) cover
specific topics in depth: working with objects, generated code, logging, dead reckoning, and more.

## Example applications

Included in the same examples directory, you can find a set of example applications:

| Application | Description |
| ----------- | ----------- |
| [Web explorer](../snippets/examples/apps/web_explorer/readme.md) | Basic Sen explorer for the web browser |
| [REST Python](../snippets/examples/apps/rest_python/readme.md) | Python client for the Sen REST component |
| [Recording inspector](../snippets/examples/config/6_recorder/readme.md#using-c-to-inspect-the-recordings) | Reads a recording with the C++ database API |

## Generated documentation

Sen writes documentation from your interface definitions, so it cannot drift from the code. For
example:

- [UML generation](generated_uml.md): class diagrams, here drawn from an HLA FOM
- [Web generation](../snippets/fom.md): a browsable interface reference, here for an HLA FOM

## Reference material

- [STL language reference](../users_guide/stl.md): full syntax of the Sen Type Language
- [Command line reference](../users_guide/command_line.md): `sen run`, `sen shell`, YAML config
  schema
- [Sen Query Language](../users_guide/sql.md): filtering objects with SQL-like expressions
