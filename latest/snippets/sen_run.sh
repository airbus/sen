Run a sen kernel

Usage: sen run [OPTIONS] [config]

Positionals:
  config TEXT:PATH(existing)       Configuration file

Options:
  -h,--help                        Print this help message and exit
  --preset TEXT:{shell,replay,explorer,web-explorer}
                                   Preset name
  --no-browser                     With --preset web-explorer: don't auto-open the URL in a browser
  --stopped                        With --preset replay: start paused
  --start-stop Excludes: --print-network-footprint
                                   Stop execution after all components are running
  --print-config Excludes: --print-network-footprint
                                   Print the configuration that will be used


Network footprint:
  --print-network-footprint Excludes: --start-stop --print-config
                                   Print the offline footprint and exit
  --bus TEXT Needs: --print-network-footprint
                                   Include a session.bus; repeat to include more
  --bus-file TEXT:FILE Needs: --print-network-footprint
                                   Read one session.bus per line; # starts a comment
  --format TEXT:{text,json} [text]  Needs: --print-network-footprint
                                   Set output format to readable text or json
