# === crash_report_tester.py ===========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

import subprocess
import json
import os
import re
import platform

def test_crash_reporter_generates_stacktrace():
    config_path = os.path.join(os.path.dirname(__file__), "config", "config.yaml")
    sen_executable = 'sen' if platform.system() == 'Windows' else './sen'

    process = subprocess.Popen([sen_executable, 'run', config_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    _, stderr = process.communicate()

    match = re.search(r"Crash report written to (.*\.json)", stderr)
    assert match is not None, f"Could not find crash report path in stderr. Stderr output:\n{stderr}"

    report_path = match.group(1).strip()
    assert os.path.exists(report_path), f"Crash report file does not exist: {report_path}"

    try:
        with open(report_path, 'r') as f:
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
