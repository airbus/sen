# === runner.py ========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import subprocess
import sys
import os
from time import sleep

def run_sen_command(arg):
    if os.name == 'nt':  # Windows
        subprocess.Popen(['sen', 'run', arg], start_new_session=True, env=os.environ.copy())
    else:  # Unix-like
        subprocess.Popen(['./sen', 'run', arg], start_new_session=True)


def main():
    if len(sys.argv) != 4:
        print("Usage: python runner.py <arg1> <arg2> <arg3>")
        sys.exit(1)

    arg1 = sys.argv[1]
    arg2 = sys.argv[2]
    arg3 = sys.argv[3]

    # Run the other 2 instances
    run_sen_command(arg1)
    run_sen_command(arg2)

    # Run the main instance for the smoke test
    os.execv(os.path.join(os.curdir, "sen"), ['sen', 'run', arg3])


if __name__ == "__main__":
    main()
