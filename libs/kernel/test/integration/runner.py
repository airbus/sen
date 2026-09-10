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
import time

# How long the supporting instances together are given to stop before they are killed
# outright. A total rather than one each, so the wait does not grow with their number.
SHUTDOWN_GRACE_SECONDS = 5

# How long the tester itself may run. This exists so a hang is reported here rather than by
# ctest: ctest kills the whole test at its own --timeout (20 s in cmake/util/test.cmake) and
# nothing below ever prints, so a hung run says only "Timeout" and names no process. The
# budget plus the grace above has to stay under that.
TESTER_BUDGET_SECONDS = int(os.environ.get("SEN_RUNNER_BUDGET_SECONDS", "12"))

# Exit status for a tester that had to be killed, distinct from anything it returns itself.
TESTER_HUNG_STATUS = 124

# An instance that ended before it was asked to makes the tester's own failure a
# consequence rather than a cause. The verdict stays the tester's either way.
INSTANCE = "runner.py: supporting instance"
TESTER = "runner.py: tester"


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


def report_threads(pid, label):
    """Says what every thread of a process is blocked in, from /proc.

    wchan is the kernel function a thread is waiting in, which separates the two hangs worth
    telling apart here: futex_wait is one of ours, a lock or a join, and ep_poll is a socket
    event that never arrived. Linux only, and never allowed to fail a test by itself.
    """
    try:
        tasks = sorted(os.listdir(f"/proc/{pid}/task"))
    except OSError:
        return  # not Linux, or it exited between the timeout and here

    blocked = {}
    for tid in tasks:
        try:
            with open(f"/proc/{pid}/task/{tid}/wchan", encoding="utf-8") as handle:
                where = handle.read().strip() or "(running)"
            with open(f"/proc/{pid}/task/{tid}/comm", encoding="utf-8") as handle:
                name = handle.read().strip()
        except OSError:
            continue
        blocked.setdefault((name, where), []).append(tid)

    print(f"{label}: {len(tasks)} threads", flush=True)
    for (name, where), tids in sorted(blocked.items(), key=lambda item: -len(item[1])):
        print(f"{label}:   {len(tids):3d} in {where:24s} [{name}] tid {tids[0]}", flush=True)


def end(instance, hard=False):
    """Ends an instance and anything it started.

    terminate() and kill() end one process. On Windows the children carry on holding
    what they inherited from it -- a listening socket, and the pipes whatever is
    reading this output waits on. taskkill walks the tree instead.
    """
    if os.name == "nt":
        subprocess.run(  # noqa: S603
            ["taskkill", "/pid", str(instance.pid), "/t", "/f"],  # noqa: S607
            check=False,
            capture_output=True,
        )
    elif hard:
        instance.kill()
    else:
        instance.terminate()


def stop(instances):
    """Stops the given instances and says how each of them ended.

    An instance is meant to be running here and to stop because this asks it. terminate()
    on one that has already gone is a no-op, so the three ways that can be untrue are only
    separable by asking first.
    """
    ended_early = {instance.pid for instance in instances if instance.poll() is not None}
    for instance in instances:
        end(instance)
    ignored_the_request = set()
    deadline = time.monotonic() + SHUTDOWN_GRACE_SECONDS
    for instance in instances:
        try:
            instance.wait(timeout=max(0.0, deadline - time.monotonic()))
        except subprocess.TimeoutExpired:
            ignored_the_request.add(instance.pid)
            report_threads(instance.pid, f"{INSTANCE} {instance.pid}")
            end(instance, hard=True)
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
        tester = subprocess.Popen([os.path.join(os.curdir, "sen"), "run", arg3])  # noqa: S603
        return tester.wait(timeout=TESTER_BUDGET_SECONDS)
    except subprocess.TimeoutExpired:
        # Popen and wait() rather than run(timeout=): run kills the process on expiry, and a
        # killed process has nothing left to say about where it was stuck.
        print(f"{TESTER}: still running after {TESTER_BUDGET_SECONDS}s", flush=True)
        report_threads(tester.pid, TESTER)
        end(tester, hard=True)
        tester.wait()
        return TESTER_HUNG_STATUS
    finally:
        stop(supporting)


if __name__ == "__main__":
    sys.exit(main())
