# === test_tap_to_junit.py =============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins what a TAP stream becomes, including the names bats' own formatter cannot write."""

from xml.etree import ElementTree

from tap_to_junit import document, parse

# Real bats 1.2.1 output, including the two names that make its junit formatter emit
# invalid XML and a failure with the diagnostic it writes underneath.
TAP = """1..4
ok 1 parse_args: --compiler=<value> equals form
not ok 2 verify_checksum: matches the BSD-style '*<file>' format too
# (in test file test_install_pipeline.bats, line 103)
#   `[ "$status" -eq 0 ]' failed
ok 3 host_arch: reports aarch64 in 0sec
ok 4 needs the network # skip no fetcher
"""


def cases(suite="test.bats", text=TAP):
    """The testcase elements the converter produces for a TAP stream."""
    return list(document(suite, parse(text.splitlines())).getroot().iter("testcase"))


def test_every_result_becomes_a_case():
    """The plan line is not a test, and the diagnostics under a failure are not either."""
    assert len(cases()) == 4


def test_a_name_with_markup_survives_and_the_file_still_parses(tmp_path):
    """This is the whole reason the converter exists: bats writes these unescaped."""
    out = tmp_path / "report.xml"
    document("test.bats", parse(TAP.splitlines())).write(out, encoding="utf-8")

    names = [case.get("name") for case in ElementTree.parse(out).getroot().iter("testcase")]

    assert "parse_args: --compiler=<value> equals form" in names
    assert "verify_checksum: matches the BSD-style '*<file>' format too" in names


def test_a_failure_carries_the_diagnostic_under_it():
    """TAP puts the reason in the comment lines that follow the result."""
    failing = [case for case in cases() if case.find("failure") is not None]

    assert len(failing) == 1
    detail = failing[0].find("failure").text
    assert "line 103" in detail
    assert '`[ "$status" -eq 0 ]\' failed' in detail


def test_a_skip_directive_is_a_skip_not_a_pass():
    """A skipped test ran nothing, so counting it as a pass would overstate the suite."""
    skipped = [case for case in cases() if case.find("skipped") is not None]

    assert [case.get("name") for case in skipped] == ["needs the network"]


def test_the_timing_bats_appends_is_not_part_of_the_name():
    """Bats writes "in 0sec" after the name; it is not what the test is called."""
    assert "host_arch: reports aarch64" in [case.get("name") for case in cases()]


def test_the_counts_on_the_suite_match_the_cases():
    """A reader that trusts the attributes and one that counts the cases must agree."""
    root = document("test.bats", parse(TAP.splitlines())).getroot()

    assert root.get("tests") == "4"
    assert root.get("failures") == "1"
    assert root.get("skipped") == "1"


def test_an_ampersand_and_a_quote_are_escaped(tmp_path):
    """The other two characters bats' formatter writes raw into an attribute."""
    out = tmp_path / "report.xml"
    text = '1..1\nok 1 handles & and "quotes" too\n'
    document("test.bats", parse(text.splitlines())).write(out, encoding="utf-8")

    names = [case.get("name") for case in ElementTree.parse(out).getroot().iter("testcase")]

    assert names == ['handles & and "quotes" too']
