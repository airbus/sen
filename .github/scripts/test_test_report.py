# === test_test_report.py ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the counting and the empty-report failure."""

import pytest
from test_report import EmptyReport, main, read_report

REPORT = """<?xml version="1.0"?>
<testsuite name="sen" tests="4">
  <testcase classname="core" name="passes"/>
  <testcase classname="core" name="fails"><failure message="boom"/></testcase>
  <testcase classname="kernel" name="errors"><error message="boom"/></testcase>
  <testcase classname="kernel" name="skipped"><skipped/></testcase>
</testsuite>
"""


def write(tmp_path, body):
    """Writes a report and returns its path."""
    path = tmp_path / "ctestReport.xml"
    path.write_text(body)
    return path


def test_counts_each_outcome(tmp_path):
    """Failures and errors both count as failed; skips are their own column."""
    total, skipped, passed, failed = read_report(write(tmp_path, REPORT))
    assert (total, skipped, passed) == (4, 1, 1)
    assert failed == ["core.fails", "kernel.errors"]


def test_a_report_without_tests_is_an_error(tmp_path):
    """A build configured without tests produces this, and it must not pass."""
    with pytest.raises(EmptyReport):
        read_report(write(tmp_path, '<?xml version="1.0"?>\n<testsuite name="sen" tests="0"/>\n'))


def test_a_missing_report_fails(tmp_path, monkeypatch):
    """The suite never ran, so there is nothing to report on."""
    monkeypatch.setattr("sys.argv", ["test_report.py", str(tmp_path / "absent.xml")])
    assert main() == 1


def test_summary_lists_the_failed_cases(tmp_path, monkeypatch, capsys):
    """The names are what a reader needs; the counts alone do not identify them."""
    monkeypatch.setattr("sys.argv", ["test_report.py", str(write(tmp_path, REPORT))])
    assert main() == 0
    out = capsys.readouterr().out
    assert "| 4 | 1 | 2 | 1 |" in out
    assert "- `core.fails`" in out
