# === test_summarise_sanitizer_reports.py ==============================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the grouping that turns a run's sanitizer output into a short report."""

import json
import sys
from pathlib import Path

from summarise_sanitizer_reports import findings, main, summarise

ROOT = Path(__file__).resolve().parents[2]

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


def test_distinct_families_are_counted_separately():
    """Two different families are two rows, and the header says how many.

    The unit is the family rather than the site: a race is identified by the
    pair of frames, so two findings sharing one anchor can still be distinct.
    """
    report = summarise(findings(RACE + LEAK))
    assert "2 sanitizer findings, 2 distinct families" in report


def test_source_locations_are_repository_relative():
    """A reader opens libs/..., not the runner's absolute build path."""
    entry = findings(RACE)[0]
    assert "libs/core/src/obj/work_queue.cpp:132" in entry["sources"]
    assert not any(where.startswith("/") for where in entry["sources"])


def test_a_finding_with_no_sen_frame_is_kept():
    """Dropping it would hide a finding in third-party or generated code."""
    entry = findings("WARNING: ThreadSanitizer: data race (pid=1)\n    #0 memcpy <null>\n")[0]
    assert entry["site"] == "(no sen:: frame)"


# Real output, kept verbatim. UBSan carries no stack frames at all unless
# print_stacktrace is on, so there is no sen:: frame to group these by; the
# source location is the only identity they have.
# From run 33289456516, artifact sanitizer-reports-address.
UBSAN = """\
/home/runner/work/sen/sen/components/replayer/src/replayed_object.cpp:374:19: runtime error: null pointer passed as argument 1, which is declared to never be null
/usr/include/string.h:44:28: note: nonnull attribute specified here
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /home/runner/work/sen/sen/components/replayer/src/replayed_object.cpp:374:19
/home/runner/work/sen/sen/libs/core/include/sen/core/base/checked_conversions.h:92:34: runtime error: nan is outside the range of representable values of type 'int'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /home/runner/work/sen/sen/libs/core/include/sen/core/base/checked_conversions.h:92:34
"""

# A race is a pair, and the pair is what identifies the family: one anchor plus
# "whatever the other side was" collapses distinct families together.
# Generated in the CI image, clang 20.1.8, trimmed to the two access sections.
PAIR = """\
WARNING: ThreadSanitizer: data race (pid=12)
  Write of size 4 at 0xffffdc139a78 by thread T2:
    #0 sen::impl::WorkQueueImpl::clear() /work/pair.cpp:3:73 (pair+0xfbbf8)
    #1 main::$_1::operator()() const /work/pair.cpp:8:26 (pair+0xfb304)

  Previous write of size 4 at 0xffffdc139a78 by thread T1:
    #0 sen::impl::WorkQueueImpl::push() /work/pair.cpp:3:50 (pair+0xfb6c4)
    #1 main::$_0::operator()() const /work/pair.cpp:7:26 (pair+0xfadac)

  Location is stack of main thread.
"""

INVERSION = """\
WARNING: ThreadSanitizer: lock-order-inversion (potential deadlock) (pid=30)
  Cycle in lock order graph: M0 (0xaaaabeb1b5a0) => M1 (0xaaaabeb1b5d0) => M0

  Mutex M1 acquired here while holding mutex M0 in thread T1:
    #0 pthread_mutex_lock <null> (dd+0x6d8bc)
    #3 sen::kernel::impl::Session::lock() /workspace/libs/kernel/src/session.cpp:41:3

  Mutex M0 previously acquired by the same thread here:
    #0 pthread_mutex_lock <null> (dd+0x6d8bc)
    #3 sen::kernel::impl::Bus::isObjectNameUsedLocally() /workspace/libs/kernel/src/bus.cpp:88:5
"""

# What a clean run writes when suppressions matched. Not a finding.
SUPPRESSIONS = """\
-----------------------------------------------------
Suppressions used:
  count      bytes template
    111       4113 strdup
     30     127448 sen::ObjectProvider::addListener
-----------------------------------------------------
"""


def test_undefined_behaviour_is_a_finding():
    """UBSan writes 'runtime error:', not 'WARNING: XSanitizer:'."""
    found = findings(UBSAN)
    assert len(found) == 2, "UBSan findings are invisible to the parser"
    assert {entry["tool"] for entry in found} == {"UndefinedBehaviorSanitizer"}


def test_undefined_behaviour_is_identified_by_its_source_location():
    """With no frames to group by, the location is the only identity."""
    found = findings(UBSAN)
    sites = {entry["site"] for entry in found}
    assert "components/replayer/src/replayed_object.cpp:374:19" in sites
    assert "libs/core/include/sen/core/base/checked_conversions.h:92:34" in sites


def test_undefined_behaviour_reaches_the_summary():
    """The lane reported 'no findings' over four of these."""
    report = summarise(findings(UBSAN))
    assert "No sanitizer findings" not in report
    assert "replayed_object.cpp" in report


def test_a_race_records_both_sides_of_the_pair():
    """A race is a pair; one anchor collapses distinct families together."""
    entry = findings(PAIR)[0]
    assert entry["sites"] == [
        "sen::impl::WorkQueueImpl::clear",
        "sen::impl::WorkQueueImpl::push",
    ]


def test_an_inversion_records_both_mutex_sites():
    """The same pairing rule holds for lock-order inversions."""
    entry = findings(INVERSION)[0]
    assert entry["kind"] == "lock-order-inversion"
    assert entry["sites"] == [
        "sen::kernel::impl::Session::lock",
        "sen::kernel::impl::Bus::isObjectNameUsedLocally",
    ]


def test_kinds_do_not_merge():
    """A vptr race and a plain race at the same site are different families."""
    kinds = {entry["kind"] for entry in findings(RACE + VPTR)}
    assert len(kinds) == 2


def test_a_suppressions_block_is_not_a_finding():
    """Every clean report file contains one; none of them is a finding."""
    assert findings(SUPPRESSIONS) == []


def test_nothing_read_is_reported_differently_from_nothing_found():
    """These looked identical, which is how a lane that saw nothing read as clean."""
    assert "No sanitizer reports were produced" in summarise([], reports=0)
    assert "No sanitizer findings in 3 report file(s)" in summarise([], reports=3)


def test_the_old_wording_survives_when_the_count_is_unknown():
    """A caller that passes no count keeps the sentence it had."""
    assert summarise([]) == "No sanitizer findings in this run.\n"


def test_it_can_be_told_to_judge_and_says_nothing_over_an_empty_run(tmp_path, monkeypatch):
    """The report files are the one place every finding can be seen at once.

    A finding written by a process ctest never waited on, or by a test whose verdict was
    inverted, reaches the files and reaches nothing else. So the lane that gates reads
    them; the nightly, which collects an inventory, must keep its exit status of zero.
    """
    reports = tmp_path / "sanitizer-reports"
    reports.mkdir()
    (reports / "report.1").write_text(SUPPRESSIONS, encoding="utf-8")
    monkeypatch.delenv("GITHUB_STEP_SUMMARY", raising=False)

    monkeypatch.setattr(sys, "argv", ["summarise", str(reports)])
    assert main() == 0, "a lane that collects must not gate"
    monkeypatch.setattr(sys, "argv", ["summarise", "--fail-on-findings", str(reports)])
    assert main() == 0, "a suppression tally is not a finding, so it must not fail a gate"

    (reports / "report.2").write_text(RACE, encoding="utf-8")
    monkeypatch.setattr(sys, "argv", ["summarise", "--fail-on-findings", str(reports)])
    assert main() == 1, "a real finding must fail the lane that gates"
    monkeypatch.setattr(sys, "argv", ["summarise", str(reports)])
    assert main() == 0, "and must still not fail the lane that collects"


def test_judging_says_nothing_about_silence(tmp_path, monkeypatch):
    """Whether a silent lane looked at anything is check_sanitizer_lane.py's question.

    Answering it here as well would turn every clean run red on a missing directory.
    """
    monkeypatch.delenv("GITHUB_STEP_SUMMARY", raising=False)
    monkeypatch.setattr(sys, "argv", ["summarise", "--fail-on-findings", str(tmp_path / "never-created")])
    assert main() == 0


def test_a_runtime_that_could_not_do_its_job_is_not_read_as_silence(tmp_path, monkeypatch):
    """With a log path set the runtime writes this into the file and nothing to stderr.

    Every test then goes red while the summary beside them said there were no findings,
    which is the worst of both: a wall of failures and a report claiming a clean run.
    """
    reports = tmp_path / "sanitizer-reports"
    reports.mkdir()
    (reports / "report.1").write_text(
        "AddressSanitizer: failed to read suppressions file '/gone/asan_ignorelist.txt'\n", encoding="utf-8"
    )
    monkeypatch.delenv("GITHUB_STEP_SUMMARY", raising=False)
    monkeypatch.setattr(sys, "argv", ["summarise", "--fail-on-findings", str(reports)])
    assert main() == 1

    text = summarise([], reports=1, broken=["AddressSanitizer: failed to read suppressions file '/gone'"])
    assert "silence says nothing" in text
    assert "No sanitizer findings" not in text


def test_a_report_that_cannot_be_read_is_not_the_same_as_one_that_is_not_there(tmp_path, monkeypatch):
    """A container running as root leaves its reports unreadable by the job's user."""
    reports = tmp_path / "sanitizer-reports"
    reports.mkdir()
    unreadable = reports / "report.1"
    unreadable.write_text("anything\n", encoding="utf-8")
    unreadable.chmod(0o000)
    monkeypatch.delenv("GITHUB_STEP_SUMMARY", raising=False)
    monkeypatch.setattr(sys, "argv", ["summarise", "--fail-on-findings", str(reports)])
    try:
        assert main() == 1
    finally:
        unreadable.chmod(0o644)


def test_a_finding_whose_path_contains_a_space_is_still_a_finding():
    """A build directory may contain one, and the pattern could not cross it."""
    entries = findings("/work/src dir/ub.cpp:2:55: runtime error: signed integer overflow\n")
    assert len(entries) == 1
    assert entries[0]["tool"] == "UndefinedBehaviorSanitizer"


def test_both_wordings_of_a_suppression_file_it_cannot_use_are_recognised(tmp_path, monkeypatch):
    """A missing file says "read"; an unreadable or malformed one says "parse".

    Only the first was matched, so the likelier half of the failure this was written for
    still printed "No sanitizer findings" while every test in the run went red.
    """
    monkeypatch.delenv("GITHUB_STEP_SUMMARY", raising=False)
    for wording in ("failed to read suppressions file '/gone'", "failed to parse suppressions."):
        reports = tmp_path / wording.split()[2].strip("'/.")
        reports.mkdir()
        (reports / "report.1").write_text(f"AddressSanitizer: {wording}\n", encoding="utf-8")
        monkeypatch.setattr(sys, "argv", ["summarise", "--fail-on-findings", str(reports)])
        assert main() == 1, wording


def test_a_malfunction_is_still_reported_when_the_run_also_found_things():
    """The warning has to survive alongside the table, not be replaced by it.

    That is when a reader most needs it: a suppression file that failed to load means the
    findings listed beside it may be noise that should have been suppressed.
    """
    text = summarise(findings(RACE), reports=1, broken=["AddressSanitizer: failed to parse suppressions."])
    assert "could not do its job" in text
    assert "sanitizer finding" in text


def test_a_directory_that_cannot_be_opened_is_not_read_as_an_empty_one(tmp_path, monkeypatch):
    """The container writes its reports as root; the job's user cannot open the directory.

    rglob walks past it without a word, so a heap-use-after-free read as "no reports".
    """
    reports = tmp_path / "sanitizer-reports"
    inner = reports / "container-0"
    inner.mkdir(parents=True)
    (inner / "report.1").write_text("WARNING: AddressSanitizer: heap-use-after-free\n", encoding="utf-8")
    inner.chmod(0o000)
    monkeypatch.delenv("GITHUB_STEP_SUMMARY", raising=False)
    monkeypatch.setattr(sys, "argv", ["summarise", "--fail-on-findings", str(reports)])
    try:
        assert main() == 1
    finally:
        inner.chmod(0o755)


# The asio diagnostic as the lane produced it [register E93]. The package directory is a
# conan cache path of the shape the image builds, because the register trimmed the real one
# to its tail -- which is the point: the tail is all an entry can match on.
ASIO_UBSAN = """\
/root/.conan2/p/asio8f0e3d1b2c4a5/p/include/asio/detail/impl/epoll_reactor.ipp:407:25: runtime error: member access within null pointer of type 'descriptor_state'
SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior /root/.conan2/p/asio8f0e3d1b2c4a5/p/include/asio/detail/impl/epoll_reactor.ipp:407:25 in
"""

# Reconstructed, NOT captured: the raw reports were deleted before they were archived, and
# only the frame list survived [register E92]. The four source lines are that list; the
# sen:: names are the functions those lines sit in. So this pins that the list matches the
# recorded evidence, and cannot pin the wording of a block nobody kept.
INTEREST_MANAGER_UAF = """\
==2951==ERROR: AddressSanitizer: heap-use-after-free on address 0x60d00000cbf8
READ of size 8 at 0x60d00000cbf8 thread T3
    #0 sen::core::detail::MoveOnlyFunctionImpl<void ()>::invoke() const /workspace/libs/core/include/sen/core/base/detail/move_only_function_impl.h:188:12
    #1 sen::core::MoveOnlyFunction<void ()>::operator()() const /workspace/libs/core/include/sen/core/base/move_only_function.h:33:12
    #2 sen::kernel::impl::RemoteInterestsManager::sendObjectUpdates() /workspace/libs/kernel/src/bus/remote_interest_manager.cpp:223:7

freed by thread T0 here:
    #2 sen::core::detail::MoveOnlyFunctionImpl<void ()>::~MoveOnlyFunctionImpl() /workspace/libs/core/include/sen/core/base/detail/move_only_function_impl.h:152:3
"""

ENTRY = {
    "tool": "AddressSanitizer",
    "kind": "heap-use-after-free",
    "where": "libs/kernel/src/bus/remote_interest_manager.cpp",
    "reason": "owned by the concurrency redesign",
    "remove-when": "that redesign merges",
}


def write_list(tmp_path, *entries):
    """A known-findings file holding the entries given."""
    path = tmp_path / "known.json"
    path.write_text(json.dumps({"findings": list(entries)}), encoding="utf-8")
    return str(path)


def run(monkeypatch, reports, *extra):
    """Runs the summariser over a directory the way a lane does."""
    monkeypatch.delenv("GITHUB_STEP_SUMMARY", raising=False)
    monkeypatch.setattr(sys, "argv", ["summarise", "--fail-on-findings", *extra, str(reports)])
    return main()


def reports_holding(tmp_path, text, name="reports"):
    """A report directory holding one file with the text given."""
    directory = tmp_path / name
    directory.mkdir()
    (directory / "report.1").write_text(text, encoding="utf-8")
    return directory


def test_a_known_finding_is_reported_and_does_not_gate(tmp_path, monkeypatch, capsys):
    """Both halves matter. Not gating is the point; still reporting is the safeguard.

    A suppression a reader cannot see is how a gate quietly stops gating.
    """
    reports = reports_holding(tmp_path, INTEREST_MANAGER_UAF)
    assert run(monkeypatch, reports) == 1, "without the list it must fail"

    assert run(monkeypatch, reports, "--known-findings", write_list(tmp_path, ENTRY)) == 0
    printed = capsys.readouterr().out
    assert "1 known finding, reported and not gating." in printed
    assert "owned by the concurrency redesign" in printed
    assert "that redesign merges" in printed


def test_a_run_whose_only_findings_are_known_does_not_report_silence(tmp_path, monkeypatch, capsys):
    """The failure this whole programme keeps finding: absence reported as success."""
    reports = reports_holding(tmp_path, INTEREST_MANAGER_UAF)
    run(monkeypatch, reports, "--known-findings", write_list(tmp_path, ENTRY))
    printed = capsys.readouterr().out
    assert "No sanitizer findings in" not in printed
    assert "No sanitizer findings beyond the 1 known one below." in printed


def test_an_entry_excuses_its_own_finding_and_nothing_beside_it(tmp_path, monkeypatch):
    """The danger is not a list that fails to match; it is one that matches too much."""
    known = write_list(tmp_path, ENTRY)

    elsewhere = reports_holding(
        tmp_path,
        "ERROR: AddressSanitizer: heap-use-after-free on address 0x1\n"
        "    #2 sen::kernel::impl::Bus::send() /workspace/libs/kernel/src/bus.cpp:99:7\n",
        name="elsewhere",
    )
    assert run(monkeypatch, elsewhere, "--known-findings", known) == 1, "the same kind in another file"

    other_kind = reports_holding(
        tmp_path,
        "ERROR: AddressSanitizer: stack-use-after-scope on address 0x1\n"
        "    #2 sen::kernel::impl::RemoteInterestsManager::sendObjectUpdates()"
        " /workspace/libs/kernel/src/bus/remote_interest_manager.cpp:223:7\n",
        name="other_kind",
    )
    assert run(monkeypatch, other_kind, "--known-findings", known) == 1, "another kind in the same file"


def test_a_known_finding_does_not_excuse_the_run_it_shares(tmp_path, monkeypatch, capsys):
    """One owned finding must not carry an unowned one through the gate with it."""
    reports = reports_holding(tmp_path, INTEREST_MANAGER_UAF + RACE)
    assert run(monkeypatch, reports, "--known-findings", write_list(tmp_path, ENTRY)) == 1
    printed = capsys.readouterr().out
    assert "1 sanitizer finding, 1 distinct family." in printed
    assert "1 known finding, reported and not gating." in printed


def test_a_list_that_cannot_be_read_is_not_read_as_an_empty_one(tmp_path, monkeypatch, capsys):
    """Empty is a valid state, so the two are indistinguishable from the outcome.

    Both make every finding gate. Only one of them is a mistake, and the run has to say so
    or the red is unexplained.
    """
    reports = reports_holding(tmp_path, INTEREST_MANAGER_UAF)
    assert run(monkeypatch, reports, "--known-findings", str(tmp_path / "absent.json")) == 1
    assert "could not be read" in capsys.readouterr().out

    malformed = tmp_path / "malformed.json"
    malformed.write_text('{"findings": {"not": "a list"}}', encoding="utf-8")
    assert run(monkeypatch, reports, "--known-findings", str(malformed)) == 1
    assert "could not be read" in capsys.readouterr().out


def test_an_entry_without_a_reason_or_a_removal_condition_excuses_nothing(tmp_path, monkeypatch, capsys):
    """An entry with no reason is how a list of two becomes a list of thirty.

    One with no removal condition never leaves. Refusing them is what keeps the list short
    enough that somebody still reads it.
    """
    reports = reports_holding(tmp_path, INTEREST_MANAGER_UAF)
    for field in ("reason", "remove-when"):
        assert run(monkeypatch, reports, "--known-findings", write_list(tmp_path, {**ENTRY, field: ""})) == 1
        assert f"is missing {field}" in capsys.readouterr().out
        (tmp_path / "known.json").unlink()


def test_an_entry_whose_file_has_gone_says_so_without_failing_the_run(tmp_path, monkeypatch, capsys):
    """The one removal condition a run can check for itself.

    This entry leaves when the concurrency redesign deletes the file. Nothing else would
    ever notice that it had, and an entry that excuses nothing is the kind that accretes.
    Reported rather than fatal: it protects nothing, so it endangers nothing.
    """
    reports = reports_holding(tmp_path, RACE)
    gone = {**ENTRY, "stale-when-gone": "libs/kernel/src/bus/deleted_by_the_redesign.cpp"}
    assert run(monkeypatch, reports, "--known-findings", write_list(tmp_path, gone)) == 1, "the race still gates"
    printed = capsys.readouterr().out
    assert "no longer exists -- remove the entry" in printed

    clean = tmp_path / "clean"
    clean.mkdir()
    assert run(monkeypatch, clean, "--known-findings", str(tmp_path / "known.json")) == 0, "a stale entry is not fatal"
    assert "no longer exists -- remove the entry" in capsys.readouterr().out


def test_the_repositorys_own_list_matches_the_findings_it_was_written_for(tmp_path, monkeypatch, capsys):
    """The list is only worth having if it matches what it names.

    An entry that names the wrong thing reads exactly like one that names the right
    thing, until the gate goes red.

    Both entries are checked against the evidence that put them there, and the entries are
    checked not to swallow the run they sit in.
    """
    known = str(ROOT / ".github" / "sanitizer-known-findings.json")
    reports = reports_holding(tmp_path, INTEREST_MANAGER_UAF + ASIO_UBSAN)
    assert run(monkeypatch, reports, "--known-findings", known) == 0, "the two known findings must not gate"

    printed = capsys.readouterr().out
    assert "2 known findings, reported and not gating." in printed
    assert "chriskohlhoff/asio#686" in printed
    assert "not a certified absence" in printed

    both = reports_holding(tmp_path, INTEREST_MANAGER_UAF + ASIO_UBSAN + RACE, name="with_a_race")
    assert run(monkeypatch, both, "--known-findings", known) == 1, "and must not carry a third finding through"


def test_two_entries_naming_one_file_stay_two_rows(tmp_path, monkeypatch, capsys):
    """Keyed on the file alone they merged, and the row carried the wrong reason.

    Nothing stops a second kind being owned in the same file, and a reader given one
    finding's reason against another finding's count has been told something untrue.
    """
    other = {**ENTRY, "kind": "stack-use-after-scope", "reason": "a different owner", "remove-when": "differently"}
    reports = reports_holding(
        tmp_path,
        INTEREST_MANAGER_UAF + "ERROR: AddressSanitizer: stack-use-after-scope on address 0x1\n"
        "    #2 sen::kernel::impl::RemoteInterestsManager::sendObjectUpdates()"
        " /workspace/libs/kernel/src/bus/remote_interest_manager.cpp:230:7\n",
    )
    assert run(monkeypatch, reports, "--known-findings", write_list(tmp_path, ENTRY, other)) == 0

    printed = capsys.readouterr().out
    assert "2 known findings, reported and not gating." in printed
    assert "owned by the concurrency redesign" in printed
    assert "a different owner" in printed
    assert "| 2 |" not in printed, "the two entries were collapsed into one row"
