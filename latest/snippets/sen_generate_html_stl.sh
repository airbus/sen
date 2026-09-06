Process STL files
Usage: sen generate html stl [OPTIONS] stl_files...

Positionals:
  stl_files TEXT:FILE ... REQUIRED STL files

Options:
  -h,--help                        Print this help message and exit
  -i,--import TEXT ...             Paths where other STL files can be found
  -b,--base-path,--base_path TEXT  Base path for including generated files
  -s,--settings TEXT:FILE          Code generation settings file
  -o,--output TEXT REQUIRED        Directory to write the reference into
  -t,--title TEXT                  Name of the model, shown on screen
