sen code generator

Usage: sen gen [OPTIONS] SUBCOMMAND

Options:
  -h,--help                        Print this help message and exit
  --expect-failure                 expects a failure in the generation process (for testing purposes)

Subcommands:
  cpp                              Generates C++ code
  uml                              generates UML diagrams
  exp_package                      generates exports symbols for a sen STL package
  py                               generates Python dataclasses
  mkdocs                           generates MKDocs documentation
  json                             generates JSON schemas

For help on specific commands run 'sen gen <command> --help'
