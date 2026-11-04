Process STL files
Usage: sen generate json package stl [OPTIONS] stl_files...

Positionals:
  stl_files TEXT:FILE ... REQUIRED STL files

Options:
  -h,--help                        Print this help message and exit
  -i,--import TEXT ...             Paths where other STL files can be found
  -b,--base-path,--base_path TEXT  Base path for including generated files
  -s,--settings TEXT:FILE          Code generation settings file
  -o,--output TEXT                 Output file
  -c,--class TEXT ...              Classes implemented by the user
