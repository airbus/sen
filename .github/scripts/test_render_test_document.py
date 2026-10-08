# === test_render_test_document.py =====================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the enumeration, and that a test name cannot become typst markup."""

import hashlib
from pathlib import Path

from render_test_document import (
    DETAIL_LIMIT,
    SPEC_NAME_LIMIT,
    Leg,
    combined_criteria_prose,
    conformance,
    coverage_section,
    coverage_tree,
    deliverables,
    display_name,
    document_information,
    is_spec,
    main,
    match_key,
    read_coverage,
    read_descriptions,
    run_started,
    short_version,
    shorten,
    split_name,
    typst_string,
)
from test_report import Case

# The third case is the one that matters: every character here is typst markup.
REPORT = """<?xml version="1.0"?>
<testsuite name="sen" tests="4">
  <testcase classname="core" name="passes" time="0.12"/>
  <testcase classname="core" name="skips"><skipped/></testcase>
  <testcase classname="kernel" name="handles #hash $dollar [bracket] and *star*" time="0.5">
    <failure message="expected 1 got 2"/>
  </testcase>
  <testcase classname="kernel" name="errors" time="1.0"><error message="boom"/></testcase>
</testsuite>
"""


def write(tmp_path, body=REPORT):
    """Writes a report and returns its path."""
    path = tmp_path / "ctestReport.xml"
    path.write_text(body)
    return path


def render_to(tmp_path, monkeypatch):
    """Runs the generator and returns the document it wrote."""
    out = tmp_path / "report.typ"
    monkeypatch.setattr(
        "sys.argv", ["render_test_document.py", str(write(tmp_path)), str(out), "clang Debug", "abc123"]
    )
    assert main() == 0
    return out.read_text()


def test_every_case_is_in_the_document(tmp_path, monkeypatch):
    """A report that lists only the failures is not an inventory."""
    document = render_to(tmp_path, monkeypatch)
    for name in ("passes", "skips", "errors"):
        assert f'name: "{name}"' in document


def test_each_outcome_is_carried(tmp_path, monkeypatch):
    """An error counts as failed, a skip stays a skip, and the rest passed."""
    document = render_to(tmp_path, monkeypatch)
    assert document.count('status: "failed"') == 2
    assert document.count('status: "skipped"') == 1
    assert document.count('status: "passed"') == 1


def test_a_name_full_of_markup_stays_data(tmp_path, monkeypatch):
    """The reason names are emitted as typst strings: this one would otherwise typeset."""
    document = render_to(tmp_path, monkeypatch)
    assert 'name: "handles #hash $dollar [bracket] and *star*"' in document


def test_the_metadata_reaches_the_document(tmp_path, monkeypatch):
    """A QA document that cannot say which build it describes is not evidence."""
    document = render_to(tmp_path, monkeypatch)
    assert '"clang Debug"' in document
    assert '"abc123"' in document


def test_ctest_names_are_split_into_suite_and_case():
    """Ctest writes the whole name into both attributes, which is what the real reports hold."""
    assert split_name("InputTest.ReadsIt", "InputTest.ReadsIt") == ("InputTest", "ReadsIt", "InputTest.ReadsIt")


def test_a_ctest_name_without_a_dot_has_no_suite():
    """A smoke test is registered as a bare name. One section each turned 113 tests into 18 pages."""
    assert split_name("tracy_smoke", "tracy_smoke") == ("", "tracy_smoke", "tracy_smoke")


def test_a_real_split_from_the_writer_is_trusted():
    """Gtest's own xml writer separates them, and then there is nothing to infer."""
    assert split_name("core", "passes") == ("core", "passes", "core.passes")


def test_the_suiteless_section_appears_once(tmp_path, monkeypatch):
    """The whole point of the grouping fix: one table, not one per test."""
    path = tmp_path / "ctest.xml"
    path.write_text(
        '<?xml version="1.0"?>\n<testsuite name="sen">'
        '<testcase classname="a_smoke" name="a_smoke" time="1"/>'
        '<testcase classname="b_smoke" name="b_smoke" time="1"/>'
        '<testcase classname="Suite.One" name="Suite.One" time="1"/>'
        "</testsuite>\n"
    )
    out = tmp_path / "out.typ"
    monkeypatch.setattr("sys.argv", ["render_test_document.py", str(path), str(out)])
    assert main() == 0
    body = out.read_text()
    assert body.count('suite: ""') == 2
    assert 'suite: "Suite"' in body


def test_a_typed_test_runtime_name_joins_to_its_source(tmp_path, monkeypatch):
    """A typed test is reported as Suite.Case<T>, which matches nothing until the type comes off."""
    source = tmp_path / "typed_test.cpp"
    source.write_text("/// @test\n/// Holds for every type.\nTYPED_TEST(Guarded, Assign)\n")
    report = tmp_path / "r.xml"
    report.write_text(
        '<?xml version="1.0"?>\n<testsuite name="sen">'
        '<testcase classname="Guarded.Assign&lt;int&gt;" name="Guarded.Assign&lt;int&gt;" time="1"/>'
        "</testsuite>\n"
    )
    out = tmp_path / "out.typ"
    monkeypatch.setattr("sys.argv", ["render_test_document.py", str(report), str(out), "c", "s", str(tmp_path)])
    assert main() == 0
    assert 'does: "Holds for every type."' in out.read_text()


def test_a_parameter_dump_is_not_part_of_the_displayed_name():
    """Ctest registers gtest's hex rendering of an unprintable parameter as part of the name."""
    raw = "Suite.Case/u8_42_to_u8  # GetParam() = (24-byte object <D0-C9 6B-D1>, 56-byte object <2A-00>)"
    assert display_name(raw) == "Suite.Case/u8_42_to_u8"


def test_a_very_long_name_is_cut():
    """One registered name runs to 613 characters, which is a log line rather than a name."""
    assert len(display_name("A" * 400)) == 93


def test_quotes_and_backslashes_are_escaped():
    """Either one unescaped ends the typst string early and breaks the compile."""
    assert typst_string('a "b" c') == '"a \\"b\\" c"'
    assert typst_string("a\\b") == '"a\\\\b"'


def test_a_long_failure_is_cut():
    """Ctest puts the whole test output in a failure, which is a log, not a document."""
    cut = shorten("x" * (DETAIL_LIMIT + 500))
    assert "cut at" in cut
    assert len(cut) < DETAIL_LIMIT + 200


def test_a_long_line_is_wrapped():
    """Raw blocks do not wrap, so an unwrapped line runs off the page."""
    assert max(len(line) for line in shorten("y" * 400).splitlines()) <= 110


def test_a_run_stopped_at_the_first_failure_is_labelled_truncated(tmp_path, monkeypatch):
    """The list is then shorter than the suite, and silence about that misleads."""
    out = tmp_path / "stopped.typ"
    monkeypatch.setattr(
        "sys.argv",
        ["render_test_document.py", str(write(tmp_path)), str(out), "cfg", "sha", "--stopped-at-first-failure"],
    )
    assert main() == 0
    body = out.read_text()
    assert "stopped at the first failure, so this enumerates less than the suite" in body
    assert "The whole suite ran." not in body


def test_failures_without_the_flag_are_not_called_truncated(tmp_path, monkeypatch):
    """A run that was not asked to stop ran whole, and calling it truncated would be a lie."""
    document = render_to(tmp_path, monkeypatch)
    assert "stopped at the first failure" not in document
    assert "The whole suite ran." in document
    assert "2 tests did not pass." in document


def test_a_clean_run_is_labelled_complete(tmp_path, monkeypatch):
    """The same sentence either way would make the label worthless."""
    out = tmp_path / "clean.typ"
    path = tmp_path / "clean.xml"
    path.write_text('<?xml version="1.0"?>\n<testsuite name="sen"><testcase classname="c" name="ok"/></testsuite>\n')
    monkeypatch.setattr("sys.argv", ["render_test_document.py", str(path), str(out)])
    assert main() == 0
    body = out.read_text()
    assert "The whole suite ran." in body
    assert "Every test passed." in body
    assert '#verdict-badge("PASSED"' in body
    assert "FAILED" not in body
    assert "stopped at the first failure" not in body


def test_the_run_time_survives_the_merge(tmp_path):
    """The merge wraps the suites in a <testsuites> with no timestamp, and the cover read nothing."""
    flat = tmp_path / "flat.xml"
    flat.write_text('<?xml version="1.0"?><testsuite name="s" timestamp="2026-10-04T08:16:58"/>')
    assert run_started(flat) == "2026-10-04T08:16:58"

    merged = tmp_path / "merged.xml"
    merged.write_text(
        '<?xml version="1.0"?><testsuites><testsuite name="s" timestamp="2026-10-04T08:16:58"/></testsuites>'
    )
    assert run_started(merged) == "2026-10-04T08:16:58"

    empty = tmp_path / "empty.xml"
    empty.write_text('<?xml version="1.0"?><testsuites/>')
    assert not run_started(empty)


def test_a_missing_report_fails(tmp_path, monkeypatch):
    """The suite never ran, so there is no document to write."""
    monkeypatch.setattr(
        "sys.argv", ["render_test_document.py", str(tmp_path / "absent.xml"), str(tmp_path / "out.typ")]
    )
    assert main() == 1


def test_an_empty_report_fails(tmp_path, monkeypatch):
    """Same reason the job summary refuses one: a configuration without tests would pass."""
    path = write(tmp_path, '<?xml version="1.0"?>\n<testsuite name="sen" tests="0"/>\n')
    monkeypatch.setattr("sys.argv", ["render_test_document.py", str(path), str(tmp_path / "out.typ")])
    assert main() == 1
    assert not Path(tmp_path / "out.typ").exists()


# --------------------------------------------------------------------------------------------------
# Coverage, and the cases a runner reports per spec file.

# The shape llvm-cov prints: the filename, then regions, functions, lines and branches, each as a
# count, a missed count and a percentage. Taken from llvm-cov-20 rather than written by hand.
COVERAGE = (
    "Filename  Regions  Missed Regions  Cover  Functions  Missed Functions  Executed"
    "  Lines  Missed Lines  Cover  Branches  Missed Branches  Cover\n"
    "-----------------------------------------------------------------------------\n"
    "a.cpp      8  5  37.50%  2  1   50.00%  40  30   25.00%  4  3  25.00%\n"
    "main.cpp   4  1  75.00%  1  0  100.00%  10   0  100.00%  2  1  50.00%\n"
    "-----------------------------------------------------------------------------\n"
    "TOTAL     12  6  50.00%  3  1   66.67%  50  30   40.00%  6  4  33.33%\n"
)


def test_coverage_is_read_from_the_total_row(tmp_path):
    """The line percentage is the tenth column, which is the one the floor is held against."""
    report = tmp_path / "coverage.txt"
    report.write_text(COVERAGE)

    totals, files = read_coverage(report)

    assert totals["Lines"] == "40.00%"
    assert totals["Functions"] == "66.67%"
    assert totals["Regions"] == "50.00%"
    assert totals["Lines uncovered"] == "30"
    assert [(f.name, f.lines, f.lines_missed, f.lines_cover) for f in files] == [
        ("a.cpp", 40, 30, 25.0),
        ("main.cpp", 10, 0, 100.0),
    ]
    # The branch columns are read past rather than reported: llvm-cov counts one source
    # conditional once per macro expansion, so the figure misleads and the document drops it.
    assert "Branches" not in totals


def test_a_report_without_branches_still_reads(tmp_path):
    """A build that did not measure branches prints ten columns, not thirteen."""
    report = tmp_path / "coverage.txt"
    report.write_text("\n".join(line.rsplit(None, 3)[0] for line in COVERAGE.splitlines()))

    totals, files = read_coverage(report)

    assert totals["Lines"] == "40.00%"
    assert [f.name for f in files] == ["a.cpp", "main.cpp"]


def test_the_section_says_when_the_floor_is_missed(tmp_path):
    """The figure alone does not say whether it passes; the floor it is held to does."""
    report = tmp_path / "coverage.txt"
    report.write_text(COVERAGE)
    totals, files = read_coverage(report)

    assert "fails the build" in coverage_section(totals, files, "80")
    assert "the build holds" in coverage_section(totals, files, "30")


def test_the_area_index_ends_with_a_total(tmp_path, monkeypatch):
    """A report that enumerates areas and never adds them up makes the reader do the arithmetic."""
    report = tmp_path / "r.xml"
    report.write_text(
        '<?xml version="1.0"?><testsuite name="sen" tests="2">'
        '<testcase classname="libs/core/test/a_test.cpp" name="A.ok"/>'
        '<testcase classname="libs/core/test/a_test.cpp" name="A.bad"><failure message="boom"/></testcase>'
        "</testsuite>"
    )
    coverage = tmp_path / "coverage.txt"
    coverage.write_text(COVERAGE)
    out = tmp_path / "out.typ"
    monkeypatch.setattr(
        "sys.argv",
        ["render_test_document.py", str(report), str(out), "x", "y", ".", f"--coverage={coverage}"],
    )
    assert main() == 0
    body = out.read_text()

    assert '..indexRow("", "Total", cases, 0)' in body
    # The row reads its figure from the empty key, so the overall percentage has to be there too.
    assert '"": (value: 40.0000, shown: "40.0%")' in body


def test_each_input_carries_the_digest_of_the_file_it_was_read_from(tmp_path):
    """The document asserts figures it did not measure; the digest is what ties them to a file."""
    report = tmp_path / "r.xml"
    report.write_text("<testsuite/>")
    expected = hashlib.sha256(report.read_bytes()).hexdigest()[:12]

    assert f"sha256 `{expected}`" in deliverables(report, None, None, None)

    report.write_text("<testsuite name='changed'/>")
    assert f"sha256 `{expected}`" not in deliverables(report, None, None, None)


def test_the_cover_names_each_document_with_its_digest(tmp_path, monkeypatch):
    """The cover lists what the document was made from; a name alone does not identify a file."""
    report = tmp_path / "r.xml"
    report.write_text(
        '<?xml version="1.0"?><testsuite name="sen" tests="1">'
        '<testcase classname="libs/core/test/a_test.cpp" name="A.ok"/>'
        "</testsuite>"
    )
    coverage = tmp_path / "coverage.txt"
    coverage.write_text(COVERAGE)
    out = tmp_path / "out.typ"
    monkeypatch.setattr(
        "sys.argv",
        ["render_test_document.py", str(report), str(out), "x", "y", ".", f"--coverage={coverage}"],
    )
    assert main() == 0
    body = out.read_text()

    assert f"r.xml  sha256 {hashlib.sha256(report.read_bytes()).hexdigest()[:12]}" in body
    assert f"coverage.txt  sha256 {hashlib.sha256(coverage.read_bytes()).hexdigest()[:12]}" in body
    # Twelve characters is not a sha256, so the document has to say somewhere which twelve.
    assert "first twelve characters of the sha256" in body


def test_a_spec_file_is_not_a_test_suite():
    """What separates a vitest or bats case from a gtest one is the file it is reported under."""
    assert is_spec("mcp_gateway_unit/test/unit/audit_log.test.ts")
    assert is_spec("installer_tests/test_parse.bats")
    assert not is_spec("KernelTest")
    assert not is_spec("libs/core/test/base/integer_compare_test.cpp")


def test_a_spec_name_is_kept_whole():
    """It is the description as well as the name, so the identifier limit would cut the sentence."""
    sentence = "connect() > " + "resolves with a Client once the wire is open and ready " * 4

    assert display_name(sentence, SPEC_NAME_LIMIT) == sentence[:SPEC_NAME_LIMIT] + "..."
    assert len(display_name(sentence)) < len(display_name(sentence, SPEC_NAME_LIMIT))


def test_descriptions_are_read_from_the_file_the_build_writes(tmp_path):
    """One tab-separated line per test; a line with no description is not a description."""
    path = tmp_path / "test-descriptions.tsv"
    path.write_text(
        "cli_archive_record\tRecords the archive the later steps read.\n"
        "blank_one\t\n"
        "no_tab_at_all\n"
        "  spaced  \t  Trimmed on both sides.  \n"
    )

    descriptions = read_descriptions(path)

    assert descriptions == {
        "cli_archive_record": "Records the archive the later steps read.",
        "spaced": "Trimmed on both sides.",
    }


def test_the_build_describes_a_test_that_has_no_annotation(tmp_path, monkeypatch):
    """A test registered in CMake has no source of its own, so the build is where it comes from."""
    report = tmp_path / "ctestReport.xml"
    report.write_text(
        '<?xml version="1.0"?><testsuite name="sen" tests="1">'
        '<testcase classname="cli_archive_record" name="cli_archive_record" time="1.0"/>'
        "</testsuite>"
    )
    descriptions = tmp_path / "test-descriptions.tsv"
    descriptions.write_text("cli_archive_record\tRecords the archive the later steps read.\n")
    out = tmp_path / "report.typ"
    monkeypatch.setattr(
        "sys.argv",
        [
            "render_test_document.py",
            str(report),
            str(out),
            "clang Debug",
            "abc123",
            f"--descriptions={descriptions}",
        ],
    )

    assert main() == 0
    assert 'does: "Records the archive the later steps read."' in out.read_text()


# A file with no branches at all: llvm-cov prints a dash where the percentage would go. Taken from
# a real report, where 138 of 451 rows look like this.
NO_BRANCHES = "apps/cli_package/util.cpp   1  0  100.00%  1  0  100.00%  24  5  79.17%  0  0  -\n"


def test_a_file_with_no_branches_is_not_dropped(tmp_path):
    """Its line counts are what the table ranks on, and rejecting the row loses them."""
    report = tmp_path / "coverage.txt"
    report.write_text(COVERAGE + NO_BRANCHES)

    _, files = read_coverage(report)

    row = next(f for f in files if f.name == "apps/cli_package/util.cpp")

    assert (row.lines, row.lines_missed, row.lines_cover) == (24, 5, 79.17)


def test_the_file_rows_account_for_the_total(tmp_path):
    """A row silently dropped leaves the table saying less than the total above it.

    The whole report, with the no-branch file counted in TOTAL as llvm-cov counts it: 40 + 10
    + 24 lines, 30 + 0 + 5 uncovered.
    """
    report = tmp_path / "coverage.txt"
    report.write_text(
        COVERAGE.replace(
            "TOTAL     12  6  50.00%  3  1   66.67%  50  30   40.00%  6  4  33.33%",
            "TOTAL     12  6  50.00%  3  1   66.67%  74  35   52.70%  6  4  33.33%",
        )
        + NO_BRANCHES
    )

    totals, files = read_coverage(report)

    assert sum(f.lines for f in files) == int(totals["Lines measured"])
    assert sum(f.lines_missed for f in files) == int(totals["Lines uncovered"])


def test_folders_aggregate_the_files_under_them(tmp_path):
    """The first page of a coverage report answers which part of the tree is thin, not which file."""
    report = tmp_path / "coverage.txt"
    report.write_text(
        "components/term/src/app.cpp      1  0  50.00%  1  0  50.00%  100  40  60.00%  4  2  50.00%\n"
        "components/term/src/form.cpp     1  0  50.00%  1  0  50.00%  100  60  40.00%  4  4   0.00%\n"
        "components/py/src/convert.cpp    1  0  50.00%  1  0  50.00%   50  10  80.00%  2  0 100.00%\n"
    )

    _, files = read_coverage(report)
    rows = coverage_tree(files)

    assert [(r.level, r.label, r.lines, r.lines_missed, r.lines_cover) for r in rows] == [
        (0, "components", 250, 110, 56.0),
        (1, "components/py", 50, 10, 80.0),
        (1, "components/term", 200, 100, 50.0),
    ]


def test_a_file_at_the_top_of_the_tree_does_not_become_its_own_folder(tmp_path):
    """Its directory is the repository root, which is every file and so says nothing."""
    report = tmp_path / "coverage.txt"
    report.write_text("main.cpp  1  0  50.00%  1  0  50.00%  10  5  50.00%  1  0  50.00%\n")

    _, files = read_coverage(report)

    assert coverage_tree(files) == []


SKIPPED_ONLY = """<?xml version="1.0"?>
<testsuite name="sen" tests="2">
  <testcase classname="c" name="c.waits"><skipped/></testcase>
  <testcase classname="c" name="c.waits_too"><skipped/></testcase>
</testsuite>
"""


def verdict_for(tmp_path, monkeypatch, body, name):
    """The document rendered from one report, with no optional input."""
    path = tmp_path / f"{name}.xml"
    path.write_text(body)
    out = tmp_path / f"{name}.typ"
    monkeypatch.setattr("sys.argv", ["render_test_document.py", str(path), str(out)])
    assert main() == 0
    return out.read_text()


def test_a_run_that_executed_nothing_does_not_pass(tmp_path, monkeypatch):
    """Skipped tests verify nothing, so a run of only skips has met no criterion."""
    document = verdict_for(tmp_path, monkeypatch, SKIPPED_ONLY, "skipped")

    assert '#verdict-badge("FAILED"' in document
    assert '#verdict-badge("PASSED"' not in document
    assert "No test executed. 2 tests were skipped, so nothing was verified." in document
    assert "Every test passed." not in document


def test_one_failure_is_not_reported_in_the_plural(tmp_path, monkeypatch):
    """A report that cannot count to one reads as untended."""
    body = (
        '<?xml version="1.0"?><testsuite name="sen" tests="2">'
        '<testcase classname="c" name="c.ok"/>'
        '<testcase classname="c" name="c.bad"><failure message="boom"/></testcase>'
        "</testsuite>"
    )
    document = verdict_for(tmp_path, monkeypatch, body, "one")

    assert "1 test did not pass." in document
    assert "1 tests" not in document


def test_a_run_with_skips_says_so_rather_than_claiming_all_passed(tmp_path, monkeypatch):
    """A run with skips has not had every test pass, whatever the failure count says."""
    body = (
        '<?xml version="1.0"?><testsuite name="sen" tests="2">'
        '<testcase classname="c" name="c.ok"/>'
        '<testcase classname="c" name="c.waits"><skipped/></testcase>'
        "</testsuite>"
    )
    document = verdict_for(tmp_path, monkeypatch, body, "mixed")

    assert "Every test that ran passed; 1 test skipped and ran nothing." in document


def test_an_unapproved_document_does_not_discuss_its_lack_of_approval():
    """Naming an absent approval authority answers the clause by undermining the document."""
    section = document_information("Airbus", "", "92a07032b")

    assert "Approval authority" not in section
    assert "Issuing organization" in section
    assert "Change history" in section


def test_an_unapproved_document_reports_the_clause_unclaimed():
    """Dropping the item must not leave the conformance table claiming it is answered."""
    table = conformance("")

    assert "[Approval authority], [Not claimed]," in table
    assert "Approval authority is not claimed" in table


def test_the_cover_identifies_the_run_rather_than_the_version(tmp_path, monkeypatch):
    """A version names a release, and which release a commit becomes is decided after this ran.

    So the cover carries what can be checked: the commit, the ref and the build that made it.
    """
    out = tmp_path / "identified.typ"
    monkeypatch.setattr(
        "sys.argv",
        [
            "render_test_document.py",
            str(write(tmp_path)),
            str(out),
            "clang Debug",
            "4f0d7c2a9b1e8d3c5a6f7b8e9d0c1a2b3c4d5e6f",
            "--ref=main",
            "--run=1482",
        ],
    )
    assert main() == 0
    document = out.read_text()

    assert '("Code revision", "4f0d7c2a9b1e8d3c5a6f7b8e9d0c1a2b3c4d5e6f")' in document
    assert '("Ref", "main")' in document
    assert '("Build", "1482")' in document
    assert '"Sen version"' not in document


def test_a_document_rendered_outside_ci_says_so(tmp_path, monkeypatch):
    """A local render has no ref and no build number, and an absent field would hide that."""
    document = render_to(tmp_path, monkeypatch)

    assert '("Ref", "not recorded")' in document
    assert '("Build", "not recorded")' in document


def test_folding_never_merges_two_tests():
    """Matching across configurations folds the compiler's spelling, never two distinct tests."""
    names = [
        "VectorTestTemplate.assign2<(anonymous namespace)::VectorTestTypes<float,10ul>>",
        "VectorTestTemplate.assign2<(anonymous namespace)::VectorTestTypes<int,10ul>>",
        "GuardedTest.Assign<int*>",
        "GuardedTest.Assign<long*>",
    ]
    assert len({match_key(name) for name in names}) == len(names)


def test_the_same_test_matches_across_compilers():
    """MSVC and gcc write the same typed test differently; they have to match all the same."""
    gcc = "VectorTestTemplate.assign2<(anonymous namespace)::VectorTestTypes<float,10ul>>"
    msvc = "VectorTestTemplate.assign2<`anonymous namespace'::VectorTestTypes<float,10>>"
    assert match_key(gcc) == match_key(msvc)
    assert match_key("GuardedTest.Assign<int * __ptr64>") == match_key("GuardedTest.Assign<int*>")


def test_a_version_is_the_number_not_the_whole_line():
    """A tool's --version line carries a number; the rest identifies nothing."""
    assert short_version("Compiler", "g++-12 (Ubuntu 12.3.0-1ubuntu1~22.04.3) 12.3.0") == "12.3.0"
    assert short_version("Compiler", "Ubuntu clang version 20.1.8 (++2025080409)") == "20.1.8"
    assert short_version("Ninja", "1.13.2.git.kitware.jobserver-pipe-1") == "1.13.2"
    assert short_version("Python", "Python 3.10.12") == "3.10.12"


def test_the_distribution_package_version_is_not_the_compiler_version():
    """g++ prints its distribution's package version in brackets; only the last one is gcc's."""
    assert short_version("Compiler", "g++-12 (Ubuntu 12.4.0-2ubuntu1~24.04.1) 12.4.0") == "12.4.0"


def test_a_value_holding_no_version_is_left_alone():
    """MSVC recorded as its own name, and anything that is not a version key, stay as they are."""
    assert short_version("Compiler", "cl") == "cl"
    assert short_version("Memory", "15.6 GiB") == "15.6 GiB"
    assert short_version("Operating system", "Ubuntu 22.04.5 LTS") == "Ubuntu 22.04.5 LTS"


def a_leg(name, cover=None, cases=None):
    """A configuration carrying only what the summary prose reads."""
    return Leg(
        configuration=name,
        report=Path("report.xml"),
        cases=cases if cases is not None else [Case("suite", "a", "passed", 0.1, "")],
        measured=None,
        coverage="",
        area_coverage={},
        area_missed={},
        line_coverage=cover,
        environment=[],
        descriptions={},
        environment_file=None,
        descriptions_file=None,
        stopped_at_first_failure=False,
    )


def test_the_summary_says_when_one_configuration_measured_coverage_alone():
    """Today only one leg instruments, and a figure from one build is not a figure for five."""
    legs = [a_leg("gcc-x86-Debug"), a_leg("clang-x86-Debug", 84.59), a_leg("msvc-x86-Release")]
    said = combined_criteria_prose(legs, 84.59, "80")
    assert "Only clang-x86-Debug measured coverage" in said
    assert "84.59% of lines" in said


def test_the_summary_reports_the_lowest_once_every_configuration_measures():
    """When they all measure, a floor claimed for the release has to hold for the worst of them."""
    legs = [a_leg("gcc-x86-Debug", 84.3), a_leg("gcc-x86-Release", 83.9), a_leg("msvc-x86-Release", 79.1)]
    said = combined_criteria_prose(legs, 79.1, "80")
    assert "lowest any configuration measured was 79.10% of lines" in said
    assert "Only" not in said


def test_the_summary_says_the_floor_was_not_tested_when_nothing_measured():
    """A floor nothing measured against was not met; it was not tested."""
    said = combined_criteria_prose([a_leg("gcc-x86-Debug"), a_leg("msvc-x86-Release")], None, "80")
    assert "No configuration measured coverage, so the floor was not tested." in said
