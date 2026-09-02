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

# An instance that ended before it was asked to makes the tester's own failure a
# consequence rather than a cause. The verdict stays the tester's either way.
INSTANCE = "runner.py: supporting instance"


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
    """Stops the given instances and says how each of them ended.

    An instance is meant to be running here and to stop because this asks it. terminate()
    on one that has already gone is a no-op, so the three ways that can be untrue are only
    separable by asking first.
    """
    ended_early = {instance.pid for instance in instances if instance.poll() is not None}
    for instance in instances:
        instance.terminate()
    ignored_the_request = set()
    for instance in instances:
        try:
            instance.wait(timeout=SHUTDOWN_GRACE_SECONDS)
        except subprocess.TimeoutExpired:
            ignored_the_request.add(instance.pid)
            instance.kill()
            instance.wait()

    for instance in instances:
        if instance.pid in ended_early:
            print(f"{INSTANCE}: {instance.pid} ended on its own with status {instance.returncode}", flush=True)
        elif instance.pid in ignored_the_request:
            print(f"{INSTANCE}: {instance.pid} ignored the request to stop and was killed", flush=True)


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
