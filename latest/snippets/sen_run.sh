Run a sen kernel

Usage: sen run [OPTIONS] [config]

Positionals:
  config TEXT:PATH(existing)       Configuration file

Options:
  -h,--help                        Print this help message and exit
  --preset TEXT:{shell,replay,explorer,web-explorer}
                                   Preset name
  --start-stop                     Stop execution after all components are running
  --no-browser                     With --preset web-explorer: don't auto-open the URL in a browser
  --print-config                   Print the configuration that will be used
