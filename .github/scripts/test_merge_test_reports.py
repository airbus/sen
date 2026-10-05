# === test_merge_test_reports.py =======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins what the fold does to a runner entry, and what it refuses to do."""

from merge_test_reports import merge

REPORT = """<?xml version="1.0"?>
<testsuite name="sen" tests="3">
  <testcase classname="core" name="core.passes" time="0.12"/>
  <testcase classname="vitest_unit" name="vitest_unit" time="9.0"/>
  <testcase classname="core" name="core.other" time="0.2"/>
</testsuite>
"""

CASES = """<?xml version="1.0"?>
<testsuites><testsuite name="vitest">
  <testcase classname="test/a.test.ts" name="connect() &gt; opens the wire" time="0.01"/>
  <testcase classname="test/a.test.ts" name="connect() &gt; closes it again" time="0.02"/>
</testsuite></testsuites>
"""


def prepare(tmp_path, report=REPORT, cases=CASES, stem="vitest_unit"):
    """Writes a report and one per-case file, and returns the paths the merge takes."""
    path = tmp_path / "ctestReport.xml"
    path.write_text(report)
    extra = tmp_path / "test-reports"
    extra.mkdir()
    (extra / f"{stem}.xml").write_text(cases)
    return path, extra


def names(tree):
    """The case names in the order the report holds them."""
    return [case.get("name") for case in tree.getroot().iter("testcase")]


def test_the_runner_entry_becomes_the_cases_behind_it(tmp_path):
    """And in its place, so the report keeps the order ctest ran things in."""
    tree, notes = merge(*prepare(tmp_path))

    assert names(tree) == [
        "core.passes",
        "connect() > opens the wire",
        "connect() > closes it again",
        "core.other",
    ]
    assert notes == ["vitest_unit: expanded into 2 cases"]


def test_the_cases_keep_the_entry_that_ran_them(tmp_path):
    """A spec path repeats across packages, so it alone would not say which one this is."""
    tree, _ = merge(*prepare(tmp_path))
    classnames = {case.get("classname") for case in tree.getroot().iter("testcase")}

    assert "vitest_unit/test/a.test.ts" in classnames


def test_a_file_without_a_ctest_entry_is_not_merged(tmp_path):
    """Left over from an earlier run: counting it would report tests this run never executed."""
    tree, notes = merge(*prepare(tmp_path, stem="some_other_suite"))

    assert names(tree) == ["core.passes", "vitest_unit", "core.other"]
    assert notes == ["some_other_suite: no such ctest entry in this run, not merged"]


def test_a_failing_runner_whose_cases_all_passed_keeps_its_entry(tmp_path):
    """It failed for a reason no case reports, and dropping the entry would drop the failure."""
    failing = REPORT.replace(
        '<testcase classname="vitest_unit" name="vitest_unit" time="9.0"/>',
        '<testcase classname="vitest_unit" name="vitest_unit" time="9.0"><failure message="exited 1"/></testcase>',
    )
    tree, notes = merge(*prepare(tmp_path, report=failing))

    assert "vitest_unit (runner)" in names(tree)
    assert notes == ["vitest_unit: 2 cases, all passed, but the runner failed"]


def test_a_failing_runner_with_a_failing_case_is_replaced(tmp_path):
    """The failure is already in the cases, so repeating it would count it twice."""
    failing_report = REPORT.replace(
        '<testcase classname="vitest_unit" name="vitest_unit" time="9.0"/>',
        '<testcase classname="vitest_unit" name="vitest_unit" time="9.0"><failure message="exited 1"/></testcase>',
    )
    failing_case = CASES.replace(
        '<testcase classname="test/a.test.ts" name="connect() &gt; closes it again" time="0.02"/>',
        '<testcase classname="test/a.test.ts" name="connect() &gt; closes it again" time="0.02">'
        '<failure message="still open"/></testcase>',
    )
    tree, notes = merge(*prepare(tmp_path, report=failing_report, cases=failing_case))

    assert "vitest_unit (runner)" not in names(tree)
    assert notes == ["vitest_unit: expanded into 2 cases"]


def test_an_empty_file_leaves_the_entry_alone(tmp_path):
    """A runner that wrote no cases says nothing about them, so the one entry is all there is."""
    tree, notes = merge(*prepare(tmp_path, cases='<?xml version="1.0"?><testsuite name="v"/>'))

    assert "vitest_unit" in names(tree)
    assert notes == ["vitest_unit: no cases in vitest_unit.xml, left as one entry"]


def test_bats_output_folds_the_same_way(tmp_path):
    """Bats reports the .bats file as the classname, as vitest does the spec."""
    bats = """<?xml version="1.0"?>
<testsuites><testsuite name="test_parse.bats">
  <testcase classname="test_parse.bats" name="parse_toolchain: clang" time="0.03"/>
</testsuite></testsuites>
"""
    report = REPORT.replace('name="vitest_unit" time="9.0"', 'name="installer_tests" time="9.0"')
    report = report.replace('classname="vitest_unit"', 'classname="installer_tests"')
    tree, notes = merge(*prepare(tmp_path, report=report, cases=bats, stem="installer_tests"))

    assert "parse_toolchain: clang" in names(tree)
    assert notes == ["installer_tests: expanded into 1 case"]


def test_a_directory_named_for_the_entry_folds_all_its_files(tmp_path):
    """Bats names each report after its .bats file, so the directory is what says where they go."""
    path = tmp_path / "ctestReport.xml"
    path.write_text(REPORT.replace("vitest_unit", "installer_tests"))
    extra = tmp_path / "test-reports"
    suite = extra / "installer_tests"
    suite.mkdir(parents=True)
    for name, case in (("test_parse", "parse_toolchain: clang"), ("test_resolve", "resolve_url: tag")):
        (suite / f"TestReport-{name}.bats.xml").write_text(
            '<?xml version="1.0"?><testsuites><testsuite name="b">'
            f'<testcase classname="{name}.bats" name="{case}" time="0"/>'
            "</testsuite></testsuites>"
        )

    tree, notes = merge(path, extra)

    assert "parse_toolchain: clang" in names(tree)
    assert "resolve_url: tag" in names(tree)
    assert notes == ["installer_tests: expanded into 2 cases"]


def test_a_directory_with_no_ctest_entry_is_not_merged(tmp_path):
    """The same staleness guard as for a single file."""
    path = tmp_path / "ctestReport.xml"
    path.write_text(REPORT)
    extra = tmp_path / "test-reports"
    suite = extra / "gone_away"
    suite.mkdir(parents=True)
    (suite / "a.xml").write_text(
        '<?xml version="1.0"?><testsuites><testsuite name="b">'
        '<testcase classname="x" name="y" time="0"/></testsuite></testsuites>'
    )

    _, notes = merge(path, extra)

    assert notes == ["gone_away: no such ctest entry in this run, not merged"]


def test_the_suite_totals_are_rewritten_after_the_fold(tmp_path):
    """Ctest wrote them for the entries it ran, and the fold has just changed every one."""
    stale = REPORT.replace(
        '<testsuite name="sen" tests="3">', '<testsuite name="sen" tests="3" failures="0" skipped="0">'
    )
    tree, _ = merge(*prepare(tmp_path, report=stale))
    root = tree.getroot()

    assert root.get("tests") == "4"
    assert root.get("failures") == "0"
    assert root.get("skipped") == "0"


# What ctest and the flaky pass produce once junitparser has merged them: the suites wrapped in a
# testsuites, which is the shape the real pipeline hands this script.
WRAPPED = """<?xml version="1.0"?>
<testsuites tests="3" failures="0">
  <testsuite name="sen" tests="3" failures="0">
    <testcase classname="core" name="core.passes" time="0.12"/>
    <testcase classname="vitest_unit" name="vitest_unit" time="9.0"/>
    <testcase classname="core" name="core.other" time="0.2"/>
  </testsuite>
</testsuites>
"""


def test_the_root_counts_the_cases_it_holds(tmp_path):
    """A reader looks at the root first, and it said 3 while the file listed 4.

    Expanding one entry into the cases behind it changes the totals, and the root carries them
    too. It is a testsuites rather than a testsuite once the passes are merged, so a guard that
    names only the singular skips the element everything reads.
    """
    report, extra = prepare(tmp_path, report=WRAPPED)

    tree, _ = merge(report, extra)
    root = tree.getroot()

    assert len(root.findall(".//testcase")) == 4
    assert root.get("tests") == "4"
    assert root.find("testsuite").get("tests") == "4"
