# === runner.py ========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to orchestrate multiple sen processes to run the test setup."""

import os
import subprocess
import sys

# How long a supporting instance is given to stop before it is killed outright.
SHUTDOWN_GRACE_SECONDS = 5


def run_sen_command(args):
    """
    Do a sen run with the given arguments and return the process.

    The caller keeps it in order to stop it: a supporting instance shuts down when the
    tester asks it to, and the tester does not always get that far.

    Args:
        args: passed to sen
    """
    if os.name == "nt":  # Windows
        return subprocess.Popen(["sen", "run", args], start_new_session=True, env=os.environ.copy())
    else:  # Unix-like
        return subprocess.Popen(["./sen", "run", args], start_new_session=True)


def stop(instances):
    """Stops the given instances, killing any that ignore the request."""
    for instance in instances:
        instance.terminate()
    for instance in instances:
        try:
            instance.wait(timeout=SHUTDOWN_GRACE_SECONDS)
        except subprocess.TimeoutExpired:
            instance.kill()


def main():
    """Run the test setup."""
    if len(sys.argv) != 4:
        print("Usage: python runner.py <arg1> <arg2> <arg3>")
        sys.exit(1)

    arg1 = sys.argv[1]
    arg2 = sys.argv[2]
    arg3 = sys.argv[3]

    # Run the other 2 instances
    supporting = [run_sen_command(arg1), run_sen_command(arg2)]

    try:
        # Run the main instance for the smoke test, as a child rather than through exec.
        # Exec replaces this process, so when the main instance dies on its own there is
        # nothing left to stop the other two. The status is still the main instance's.
        return subprocess.run([os.path.join(os.curdir, "sen"), "run", arg3], check=False).returncode
    finally:
        stop(supporting)


if __name__ == "__main__":
    sys.exit(main())
