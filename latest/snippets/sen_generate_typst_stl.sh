Process STL files
Usage: sen generate typst stl [OPTIONS] stl_files...

Positionals:
  stl_files TEXT:FILE ... REQUIRED STL files

Options:
  -h,--help                        Print this help message and exit
  -i,--import TEXT ...             Paths where other STL files can be found
  -b,--base-path,--base_path TEXT  Base path for including generated files
  -s,--settings TEXT:FILE          Code generation settings file
  -o,--output TEXT REQUIRED        Directory to write the document into
  -t,--title TEXT                  Name of the model, shown on the running header
  --include-package TEXT ...       Only document this package; repeatable
  --exclude-package TEXT ...       Leave this package out; repeatable
  --no-overview{false}             Drop the model overview
  --no-hierarchy{false}            Drop the class hierarchy
  --no-summaries{false}            Drop the per-package summary lists
  --no-index{false}                Drop the index of every type
  --no-used-by{false}              Drop the list of what refers to each type
  --no-flag-legend{false}          Drop the explanation of the property flags
  --no-built-ins{false}            Drop the list of the language's built-in types
  --front-matter TEXT:FILE         Typst file to include before the reference, as a title page
  --before-reference TEXT:FILE     Typst file to include after the front matter
  --after-reference TEXT:FILE      Typst file to include after the reference
  --style TEXT:FILE                Typst file defining the look, replacing the one shipped
