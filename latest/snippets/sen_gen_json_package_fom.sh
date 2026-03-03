process HLA FOM files
Usage: sen gen json package fom [OPTIONS]

Options:
  -h,--help                        Print this help message and exit
  -m,--mappings TEXT:FILE ...      XML defining custom mappings between sen and hla
  -d,--directories TEXT:DIR ... REQUIRED
                                   directories containing FOM XML files
  -s,--settings TEXT:FILE          code generation settings file
  -o,--output TEXT                 output file
  -c,--class TEXT ...              Classes implemented by the user
