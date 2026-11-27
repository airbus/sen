combine multiple archives into one
Usage: sen archive merge [OPTIONS] input_paths...

Positionals:
  input_paths TEXT:DIR ... REQUIRED
            Recording path

Options:
  -h,--help Print this help message and exit
  -o,--output TEXT
            Output recording path
  --mode TEXT:{normal,zero,offset}
            Merge mode
  --offset FLOAT ...
            offset for each input recording (in seconds)
  --force   Replace a recording archive already present at the output path
