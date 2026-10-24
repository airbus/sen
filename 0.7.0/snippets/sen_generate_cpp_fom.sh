Process HLA FOM files
Usage: sen generate cpp fom [OPTIONS]

Options:
  -h,--help                        Print this help message and exit
  -m,--mappings TEXT:FILE ...      XML defining custom mappings between sen and HLA
  -d,--directories TEXT:DIR ... REQUIRED
                                   Directories containing FOM XML files
  -e,--extensions TEXT:PATH(existing) ...
                                   XML files, or directories of them, adding members to types the FOM declares
  -s,--settings TEXT:FILE          Code generation settings file
