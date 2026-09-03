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

# How long to wait for the kernel to block the termination signals. Startup is dynamic linking and
# an exec, both of which slow down badly when the rest of the suite is starting at the same time.
READY_TIMEOUT_SECONDS = 30

# Positions of SIGTERM and SIGINT in the mask /proc reports, which counts signals from one.
BLOCKED_MASK = (1 << (signal.SIGTERM - 1)) | (1 << (signal.SIGINT - 1))


def blocked_signals(pid):
    """The process's blocked-signal mask, or None where /proc does not exist."""
    try:
        with open(f"/proc/{pid}/status", encoding="utf-8") as status:
            for line in status:
                if line.startswith("SigBlk:"):
                    return int(line.split()[1], 16)
    except OSError:
        return None
    return None


def wait_until_ready(kernel, fallback_delay):
    """Wait until the kernel has blocked the termination signals, so one can be delivered to it.

    A signal arriving before it does that kills the process outright, which is indistinguishable
    from the defect this test exists to catch. Sleeping a fixed time instead only looks equivalent:
    under a full parallel suite the process needs seconds to reach main, and the test then reports
    a defect that is not there. Where /proc is unavailable there is nothing to observe, so the
    fixed wait is all that is left.
    """
    if blocked_signals(kernel.pid) is None:
        time.sleep(fallback_delay)
        return True

    deadline = time.monotonic() + READY_TIMEOUT_SECONDS
    while time.monotonic() < deadline:
        mask = blocked_signals(kernel.pid)
        if mask is None:  # exited already; let the caller report what it did
            return True
        if (mask & BLOCKED_MASK) == BLOCKED_MASK:
            return True
        time.sleep(0.02)

    return False


def run_until_signalled(config, sig, delay):
    """Start a kernel, signal it once it is ready, and return (exit code, seconds taken)."""
    kernel = subprocess.Popen(["./sen", "run", config], start_new_session=True)  # noqa: S603
    if not wait_until_ready(kernel, delay):
        kernel.kill()
        kernel.wait()
        return "not ready", 0.0

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

    if code == "not ready":
        print(f"FAIL {name}: never blocked the termination signals within {READY_TIMEOUT_SECONDS}s")
        return False

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
