# === terminate_test.py ================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that a termination request stops the kernel instead of killing it."""

import signal
import subprocess
import sys
import time

# How long the kernel is given to stop. Generous: what is being tested is that it stops at all, and
# a tight bound here would turn a slow machine into a failure.
STOP_TIMEOUT_SECONDS = 15


def run_until_signalled(config, sig, delay):
    """Start a kernel, signal it after delay seconds, and return (exit code, seconds taken)."""
    kernel = subprocess.Popen(["./sen", "run", config], start_new_session=True)  # noqa: S603
    time.sleep(delay)

    start = time.monotonic()
    kernel.send_signal(sig)
    try:
        return kernel.wait(timeout=STOP_TIMEOUT_SECONDS), time.monotonic() - start
    except subprocess.TimeoutExpired:
        kernel.kill()
        kernel.wait()
        return None, time.monotonic() - start


def check(config, name, sig, delay):
    """Report whether one case stopped cleanly."""
    code, took = run_until_signalled(config, sig, delay)

    if code is None:
        print(f"FAIL {name}: still running {took:.1f}s after the request, had to be killed")
        return False

    # A negative code is python for "killed by a signal", which is the whole defect: the process
    # died where it stood instead of stopping.
    if code < 0:
        print(f"FAIL {name}: killed by signal {-code} rather than stopping")
        return False

    if code != 0:
        print(f"FAIL {name}: stopped with exit code {code}, expected 0")
        return False

    print(f"ok   {name}: stopped cleanly in {took:.2f}s")
    return True


def main():
    """Run every case and fail if any of them did."""
    if len(sys.argv) != 2:
        sys.exit("Usage: python terminate_test.py <config_yaml>")

    config = sys.argv[1]

    # Not covered: a request arriving while the kernel is still being built. This kernel loads
    # nothing, so it is built in under a millisecond and there is no window to aim at. Covering it
    # needs a kernel slow enough to start.
    results = [
        check(config, "SIGTERM while running", signal.SIGTERM, 1.0),
        check(config, "SIGINT while running", signal.SIGINT, 1.0),
    ]

    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
