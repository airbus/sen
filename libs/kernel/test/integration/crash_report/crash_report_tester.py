# === crash_report_tester.py ===========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to specify the crash reporter test cases."""

import glob
import json
import os
import platform
import re
import subprocess
import tempfile
import time


def test_crash_reporter_generates_stacktrace():
    """Test to ensure that the crash reporter generates a stacktrace on failure."""
    config_path = os.path.join(os.path.dirname(__file__), "config", "config.yaml")
    sen_executable = "sen" if platform.system() == "Windows" else "./sen"

    process = subprocess.Popen(
        [sen_executable, "run", config_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    _, stderr = process.communicate()

    match = re.search(r"Crash report written to (.*\.json)", stderr)
    assert match is not None, f"Could not find crash report path in stderr. Stderr output:\n{stderr}"

    report_path = match.group(1).strip()
    assert os.path.exists(report_path), f"Crash report file does not exist: {report_path}"

    try:
        with open(report_path, encoding="utf-8") as f:
            data = json.load(f)

        error_data = data.get("errorData", {})
        process_data = data.get("processData", {})

        assert "bad_weak_ptr" in str(error_data), "bad_weak_ptr not found in the crash report"

        stacktrace = process_data.get("stacktrace")
        assert stacktrace is not None, "Stacktrace field is null"
        assert len(stacktrace) > 0, "Stacktrace is empty"

    finally:
        if os.path.exists(report_path):
            os.remove(report_path)


def test_crash_reporter_records_a_signal():
    """A signal is recorded by the handler itself, which may not allocate.

    The rich report cannot be produced from a signal handler, so the handler writes a small record
    beside the context file and re-raises. This asserts the record arrives and names the signal,
    because the path that produces it had no test and is the one that used to hang.
    """
    if platform.system() == "Windows":
        return  # the handler is Linux-only; Windows has none at all, which is its own item

    config_path = os.path.join(os.path.dirname(__file__), "config", "config.yaml")
    pattern = os.path.join(tempfile.gettempdir(), "*.sen-crash")
    for stale in glob.glob(pattern):
        os.remove(stale)

    environment = dict(os.environ, SEN_CRASH_MODE="signal")
    process = subprocess.Popen(
        ["./sen", "run", config_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=environment
    )
    process.communicate()

    # The process dies of the signal rather than of _Exit, so the status is negative.
    assert process.returncode < 0, f"expected death by signal, got status {process.returncode}"

    deadline = time.monotonic() + 5.0
    records = []
    while time.monotonic() < deadline and not records:
        records = glob.glob(pattern)
        if not records:
            time.sleep(0.1)

    assert records, f"the handler wrote no crash record matching {pattern}"

    with open(records[0], encoding="utf-8") as handle:
        content = handle.read()

    assert content.startswith("sen-crash "), f"unexpected record format:\n{content}"
    assert "signal 0xb" in content, f"SIGSEGV not named in the record:\n{content}"

    # Which part of its life the kernel was in: a crash while stopping and one while running point at
    # different code. This one crashes after startup, so "running" -- deliberately not the value the
    # variable would hold if nothing ever set it, which would pass whether or not this works.
    assert "\nphase 0x1\n" in content, f"the record does not say the kernel was running:\n{content[:300]}"
    assert "frame 0x" in content, f"no addresses recorded:\n{content}"

    # Position independent binaries land somewhere different every run, so the addresses above are
    # unresolvable without knowing where each module started.
    assert "\nmaps\n" in content, f"no module map in the record:\n{content[:400]}"
    assert "libkernel" in content, f"the module map names no Sen library:\n{content[:400]}"

    # The record carries only what a handler may write, so everything else has to be in the context
    # written beforehand. Without this the change is a downgrade and the record alone would hide it.
    contexts = glob.glob(os.path.join(tempfile.gettempdir(), "*.sen-context.json"))
    assert contexts, "no crash context was written before the crash"

    with open(contexts[0], encoding="utf-8") as handle:
        context = json.load(handle)

    assert context["senData"]["version"], "the context names no Sen version"


def test_crash_context_is_removed_on_a_clean_shutdown():
    """A context left behind by a run that ended cleanly describes a crash that never happened.

    uninstall() removes it from ~KernelImpl. Untested, that fails silently and every clean run
    leaves one more behind.
    """
    if platform.system() == "Windows":
        return  # no signal handler on Windows, so no context either

    config_path = os.path.join(os.path.dirname(__file__), "config", "config.yaml")
    pattern = os.path.join(tempfile.gettempdir(), "*.sen-context.json")
    for stale in glob.glob(pattern):
        os.remove(stale)

    environment = dict(os.environ, SEN_CRASH_MODE="none")
    subprocess.run(
        ["./sen", "run", "--start-stop", config_path],
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
        env=environment,
    )

    leftover = glob.glob(pattern)
    assert not leftover, f"a clean run left a crash context behind: {leftover}"
