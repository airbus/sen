Process HLA FOM files
Usage: sen generate uml fom [OPTIONS]

Options:
  -h,--help                        Print this help message and exit
  -m,--mappings TEXT:FILE ...      XML defining custom mappings between sen and HLA
  -d,--directories TEXT:DIR ... REQUIRED
                                   Directories containing FOM XML files
  -s,--settings TEXT:FILE          Code generation settings file
  -o,--output TEXT                 Output PlantUML file
  --only-classes Excludes: --only-types --no-enumerators
                                   Only generate class diagrams
  --only-types Excludes: --only-classes
                                   Only generate diagrams for basic types
  --no-enumerators Excludes: --only-classes
                                   Do not generate enumerations
