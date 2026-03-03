process STL files
Usage: sen gen mkdocs stl [OPTIONS] stl_files...

Positionals:
  stl_files TEXT:FILE ... REQUIRED STL files

Options:
  -h,--help                        Print this help message and exit
  -i,--import TEXT ...             Paths where other STL files can be found
  -b,--base_path TEXT              base path for including generated files
  -s,--settings TEXT:FILE          code generation settings file
  -o,--output TEXT                 output file
  -t,--title TEXT                  document title
