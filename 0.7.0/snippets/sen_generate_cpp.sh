Generate C++ code
Usage: sen generate cpp [OPTIONS] SUBCOMMAND

Options:
  -h,--help                        Print this help message and exit
  -r,--recursive BOOLEAN           Recursively generate imported packages
  --public-symbols                 Make generated classes public

Subcommands:
  stl                              Process STL files
  fom                              Process HLA FOM files
  exports                          Generate the export symbols file for a sen STL package
