# === crash_report_tester.py ===========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to specify the crash reporter test cases.

How Sen answers a crash depends on how the crash arrives. An uncaught exception reaches the
terminate handler where allocating is still legal, so Sen writes the report itself. A fault never
reaches Sen at all: the handler writes a minidump from outside the process that died.
"""

import json
import os
import platform
import re
import signal
import subprocess

import pytest

SEN = "sen" if platform.system() == "Windows" else "./sen"


def write_config(directory, generate_signal=None, disabled=False):
    """Writes a config that loads the crashing component, with its files kept in `directory`."""
    lines = ["kernel:", f"  crashReportDir: {directory}"]
    if disabled:
        lines.append("  crashReportDisabled: true")
    lines += [
        "",
        "build:",
        "  - name: crash_runner",
        "    group: 1",
        "    freqHz: 5",
        "    imports: [crash_report]",
        "    objects:",
        "      - name: crasher",
        "        class: sen.test.crash_report.CrashMakerImpl",
        "        bus: local.crashers",
    ]
    if generate_signal is not None:
        lines.append(f"        generateSignal: {str(generate_signal).lower()}")

    path = os.path.join(directory, "crash.yaml")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return path


def run_sen(config_path):
    """Runs the configuration to its crash and returns the finished process.

    Both streams come back. Sen's logger writes to stdout and Crashpad's handler to stderr, so a
    caller that keeps one of them cannot tell a reporter that never armed from one that armed and
    wrote nothing.
    """
    process = subprocess.Popen([SEN, "run", config_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    stdout, stderr = process.communicate()
    return process.returncode, stdout, stderr


def both_streams(stdout, stderr, directory=None):
    """The two streams laid out for an assertion message, with the database if one is given.

    Sen's logger writes to stdout and Crashpad's handler to stderr, so a caller that keeps one of
    them cannot tell a reporter that never armed from one that armed and wrote nothing. The
    database listing separates that again from a handler that wrote somewhere this test does not
    look: Crashpad writes into `new` and moves the file to `pending`, and creates neither until it
    has a dump to put there.
    """
    text = f"--- stdout ---\n{stdout}\n--- stderr ---\n{stderr}"
    if directory is not None:
        text += f"\n--- files under {directory} ---\n{describe_tree(directory)}"
    return text


def describe_tree(directory):
    """Every file under `directory` with its size, or a note saying there are none."""
    lines = []
    for root, _, names in os.walk(directory):
        for name in sorted(names):
            path = os.path.join(root, name)
            try:
                lines.append(f"{os.path.getsize(path):>10}  {os.path.relpath(path, directory)}")
            except OSError as error:
                lines.append(f"{'?':>10}  {os.path.relpath(path, directory)} ({error})")
    return "\n".join(lines) if lines else "(nothing)"


def find_dumps(directory):
    """The minidumps Crashpad has written. It keeps them under `pending` when nothing uploads."""
    pending = os.path.join(directory, "pending")
    if not os.path.isdir(pending):
        return []
    return [os.path.join(pending, name) for name in os.listdir(pending) if name.endswith(".dmp")]


def read_report(stderr):
    """The report Sen says it wrote, as parsed json."""
    match = re.search(r"Crash report written to (.*\.json)", stderr)
    assert match is not None, f"Could not find crash report path in stderr. Stderr output:\n{stderr}"

    report_path = match.group(1).strip()
    assert os.path.exists(report_path), f"Crash report file does not exist: {report_path}"
    try:
        with open(report_path, encoding="utf-8") as f:
            return json.load(f)
    finally:
        os.remove(report_path)


def test_crash_reporter_no_signal(tmp_path):
    """An uncaught exception is reported by Sen itself, with the stack that reached the handler."""
    returncode, stdout, stderr = run_sen(write_config(tmp_path, generate_signal=False))
    data = read_report(stderr)

    error_data = data.get("errorData", {})
    process_data = data.get("processData", {})

    exception_data = error_data.get("exceptionData")
    assert exception_data is not None, "exceptionData is null"
    assert "bad_weak_ptr" in str(exception_data), "bad_weak_ptr not found in exceptionData"

    stacktrace = process_data.get("stacktrace")
    assert stacktrace is not None, "Stacktrace field is null"
    assert len(stacktrace) > 0, "Stacktrace is empty"

    for elem in stacktrace:
        assert re.search(r"#\d+\s+0x[0-9a-f]+.+", elem) is not None, "Stack trace has an invalid format"

    # The report is not all of it: this route ends through abort(), so the handler dumps it too,
    # and the dump carries the other threads' stacks that the report does not.
    assert returncode == -signal.SIGABRT, f"expected to be killed by SIGABRT, got {returncode}"
    assert len(find_dumps(tmp_path)) == 1, (
        f"an uncaught exception should also produce a minidump.\n{both_streams(stdout, stderr, tmp_path)}"
    )


def test_crash_reporter_signal(tmp_path):
    """A fault leaves a minidump, written by another process, and kills this one as it would have."""
    if platform.system() == "Windows":
        pytest.skip("raise(SIGFPE) on Windows is not the fault Crashpad catches there")

    returncode, stdout, stderr = run_sen(write_config(tmp_path, generate_signal=True))

    assert returncode == -signal.SIGFPE, f"expected to be killed by SIGFPE, got {returncode}"
    assert "Crash report written to" not in stderr, "a fault should not reach Sen's own reporting"

    dumps = find_dumps(tmp_path)
    assert len(dumps) == 1, (
        f"expected one minidump under {tmp_path}, found {dumps}.\n{both_streams(stdout, stderr, tmp_path)}"
    )
    assert os.path.getsize(dumps[0]) > 0, "the minidump is empty"

    # The annotations reach the handler through an ELF note that the linker drops unless it is
    # asked for it by name, and a dump without them looks the same in every other way.
    with open(dumps[0], "rb") as f:
        dump = f.read()
    assert b"sen.phase" in dump, "the dump carries no phase annotation"
    assert b"sen.components" in dump, "the dump carries no component annotation"
    assert b"crash_runner" in dump, "the component inventory did not reach the dump"


def test_no_handler_program_is_installed():
    """No crashpad_handler program is installed beside the binaries."""
    for name in ("crashpad_handler", "crashpad_handler.exe"):
        assert not os.path.exists(name), (
            f"{name} is installed beside the binaries; the kernel is meant to run the handler itself"
        )


def test_crash_reporter_no_report(tmp_path):
    """Setting crashReportDisabled arms nothing, so neither route leaves anything behind."""
    _, stdout, stderr = run_sen(write_config(tmp_path, generate_signal=False, disabled=True))

    assert "Crash report written to" not in stderr, f"A crash report was written. Stderr output:\n{stderr}"
    assert find_dumps(tmp_path) == [], (
        f"a disabled reporter should not produce a minidump.\n{both_streams(stdout, stderr, tmp_path)}"
    )
