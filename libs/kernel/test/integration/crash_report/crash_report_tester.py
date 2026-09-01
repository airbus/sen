# === crash_report_tester.py ===========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Module to specify the crash reporter test cases."""

import json
import os
import platform
import re
import subprocess


def test_crash_reporter_no_signal():
    """Test to ensure that the crash reporter generates a report when package crashes without a signal."""
    config_path = os.path.join(os.path.dirname(__file__), "config", "no_signal.yaml")
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

        exception_data = error_data.get("exceptionData")
        assert exception_data is not None, "exceptionData is null"
        assert "bad_weak_ptr" in str(exception_data), "bad_weak_ptr not found in exceptionData"

        stacktrace = process_data.get("stacktrace")
        assert stacktrace is not None, "Stacktrace field is null"
        assert len(stacktrace) > 0, "Stacktrace is empty"

        for elem in stacktrace:
            match = re.search(r"#\d+\s+0x[0-9a-f]+.+", elem)
            assert match is not None, "Stack trace has an invalid format"

        signal_data = error_data.get("signalData")
        assert signal_data is None, "Signal was provided when it was not expected"

    finally:
        if os.path.exists(report_path):
            os.remove(report_path)


def test_crash_reporter_signal():
    """Test to ensure that the crash reporter generates a report when package crashes with a signal."""
    config_path = os.path.join(os.path.dirname(__file__), "config", "signal.yaml")
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

        exception_data = error_data.get("exceptionData")
        assert exception_data is None, "There is exceptionData"

        stacktrace = process_data.get("stacktrace")
        assert stacktrace is not None, "Stacktrace field is null"
        assert len(stacktrace) > 0, "Stacktrace is empty"

        signal_data = error_data.get("signalData")
        assert signal_data is not None, "Signal was not provided when expected"

        signal_number = signal_data.get("signalNumber")
        signal_name = signal_data.get("signalName")
        assert signal_number == 8, f"Expected signalNumber to be 8, got: {signal_number}"
        assert signal_name == "Floating point exception", (
            "Expected signalName to be " "'Floating point exception', " "got: {signal_number}"
        )

    finally:
        if os.path.exists(report_path):
            os.remove(report_path)


def test_crash_reporter_no_report():
    """Test to ensure that the crash reporter does not generate a report when crashReportDisabled flag is enabled."""
    config_path = os.path.join(os.path.dirname(__file__), "config", "no_report.yaml")
    sen_executable = "sen" if platform.system() == "Windows" else "./sen"

    process = subprocess.Popen(
        [sen_executable, "run", config_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )
    _, stderr = process.communicate()

    match = re.search(r"Crash report written to (.*\.json)", stderr)
    assert match is None, "This message should not exist"
