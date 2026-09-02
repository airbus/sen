Process STL files
Usage: sen generate ts stl [OPTIONS] stl_files...

Positionals:
  stl_files TEXT:FILE ... REQUIRED STL files

Options:
  -h,--help                        Print this help message and exit
  -i,--import TEXT ...             Paths where other STL files can be found
  -b,--base-path,--base_path TEXT  Base path for including generated files
  -s,--settings TEXT:FILE          Code generation settings file
  -d,--out-dir TEXT REQUIRED       Output directory for the generated .ts files
