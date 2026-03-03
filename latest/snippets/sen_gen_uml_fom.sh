process HLA FOM files
Usage: sen gen uml fom [OPTIONS]

Options:
  -h,--help                        Print this help message and exit
  -m,--mappings TEXT:FILE ...      XML defining custom mappings between sen and hla
  -d,--directories TEXT:DIR ... REQUIRED
                                   directories containing FOM XML files
  -s,--settings TEXT:FILE          code generation settings file
  -o,--output TEXT                 output plantuml file
  --only-classes Excludes: --only-types --no-enumerators
                                   only generate class diagrams
  --only-types Excludes: --only-classes
                                   only generate diagrams for basic types
  --no-enumerators Excludes: --only-classes
                                   do not generate enumerations
