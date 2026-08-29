# === test_summarise_sanitizer_reports.py ==============================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the grouping that turns a run's sanitizer output into a short report."""

from summarise_sanitizer_reports import findings, summarise

RACE = """\
WARNING: ThreadSanitizer: data race (pid=118)
  Write of size 8 at 0x001 by thread T1:
    #0 memcpy <null>
    #3 sen::impl::WorkQueueImpl::clear() /workspace/libs/core/src/obj/work_queue.cpp:132:5
    #5 sen::kernel::impl::Runner::stopThread() /workspace/libs/kernel/src/runner.cpp:428:14
SUMMARY: ThreadSanitizer: data race work_queue.cpp:132
"""

VPTR = """\
WARNING: ThreadSanitizer: data race on vptr (ctor/dtor vs virtual call) (pid=119)
  Read of size 8 at 0x002 by thread T1:
    #3 sen::impl::WorkQueueImpl::clear() /workspace/libs/core/src/obj/work_queue.cpp:132:5
"""

LEAK = """\
ERROR: AddressSanitizer: heap-use-after-free on address 0x003
    #2 sen::kernel::impl::Executor::shutDown() /workspace/libs/kernel/src/executor.cpp:195:15
"""


def test_no_output_means_no_findings():
    """A clean run says so rather than rendering an empty table."""
    assert findings("") == []
    assert "No sanitizer findings" in summarise([])


def test_each_warning_is_one_finding():
    """Findings are split at the WARNING/ERROR line, not at blank lines."""
    assert len(findings(RACE + VPTR + LEAK)) == 3


def test_the_site_is_the_first_sen_frame():
    """The frames above it are the sanitizer's own and name no owner."""
    assert findings(RACE)[0]["site"] == "sen::impl::WorkQueueImpl::clear"


def test_the_tool_and_kind_are_kept_apart():
    """A vptr race is a different finding from a plain one at the same site."""
    entries = findings(RACE + VPTR)
    assert [entry["kind"] for entry in entries] == ["data race", "data race on vptr"]
    assert {entry["site"] for entry in entries} == {"sen::impl::WorkQueueImpl::clear"}


def test_address_sanitizer_is_recognised_too():
    """The report is not thread-specific: ASan writes ERROR rather than WARNING."""
    entry = findings(LEAK)[0]
    assert entry["tool"] == "AddressSanitizer"
    assert entry["kind"] == "heap-use-after-free"


def test_repeats_of_one_site_collapse_into_a_count():
    """The point of the report: one defect reached by many tests is one row."""
    report = summarise(findings(RACE * 5))
    assert "| 5 |" in report
    assert report.count("WorkQueueImpl::clear") >= 1


def test_distinct_sites_are_counted_separately():
    """Two different sites are two rows, and the header says how many."""
    report = summarise(findings(RACE + LEAK))
    assert "2 sanitizer findings, 2 distinct sites" in report


def test_source_locations_are_repository_relative():
    """A reader opens libs/..., not the runner's absolute build path."""
    entry = findings(RACE)[0]
    assert "libs/core/src/obj/work_queue.cpp:132" in entry["sources"]
    assert not any(where.startswith("/") for where in entry["sources"])


def test_a_finding_with_no_sen_frame_is_kept():
    """Dropping it would hide a finding in third-party or generated code."""
    entry = findings("WARNING: ThreadSanitizer: data race (pid=1)\n    #0 memcpy <null>\n")[0]
    assert entry["site"] == "(no sen:: frame)"
