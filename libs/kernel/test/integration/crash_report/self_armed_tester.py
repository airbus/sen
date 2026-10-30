# === self_armed_tester.py =============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to specify the crash reporter test cases for a process that arms itself.

An embedder that calls crash::arm() itself has a reporter whatever the kernel configuration says,
so crashReportDisabled suppresses the report rather than the arming. `sen run` never reaches this
case: it does not arm when the key is set.
"""

import os
import signal
import subprocess

from crash_report_tester import both_streams

PROGRAM = os.environ.get("SEN_SELF_ARMED_CRASH")

# The program throws this, and the terminate handler prints it whether or not a report is written,
# so it is the witness that the handler ran at all.
BANNER = "self_armed_crash is meant to end here"


def run_program(report_directory, mode):
    """Runs the self-arming program to its crash and returns the finished process."""
    assert PROGRAM is not None, "SEN_SELF_ARMED_CRASH is unset, so this test has no program to run"
    process = subprocess.Popen(
        [PROGRAM, str(report_directory), mode], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    stdout, stderr = process.communicate()
    return process.returncode, stdout, stderr


def json_reports(directory):
    """The reports Sen wrote itself, which sit in the directory it armed with.

    Crashpad's dumps go into subdirectories of the same place, so only this level is Sen's own.
    """
    return sorted(name for name in os.listdir(directory) if name.endswith(".json"))


def test_self_armed_report_is_written(tmp_path):
    """A self-armed process writes the report into the directory it armed with."""
    returncode, stdout, stderr = run_program(tmp_path, "enabled")
    streams = both_streams(stdout, stderr, tmp_path)

    assert BANNER in stderr, f"the terminate handler did not run.\n{streams}"
    assert "Crash report written to" in stderr, f"no report was written.\n{streams}"
    assert len(json_reports(tmp_path)) == 1, f"expected exactly one report.\n{streams}"
    assert returncode == -signal.SIGABRT, f"expected to be killed by SIGABRT, got {returncode}"


def test_self_armed_report_is_suppressed(tmp_path):
    """The key suppresses that report. The dump is not affected.

    By the time the kernel reads its configuration the handler is already running in another
    process, which nothing here can call off.
    """
    returncode, stdout, stderr = run_program(tmp_path, "disabled")
    streams = both_streams(stdout, stderr, tmp_path)

    # Asserted before the absences below, which a process that died on its way to the handler would
    # satisfy just as well.
    assert BANNER in stderr, f"the terminate handler did not run.\n{streams}"

    assert "Crash report written to" not in stderr, f"the report was written anyway.\n{streams}"
    assert json_reports(tmp_path) == [], f"a report file was left behind.\n{streams}"
    assert returncode == -signal.SIGABRT, f"expected to be killed by SIGABRT, got {returncode}"
