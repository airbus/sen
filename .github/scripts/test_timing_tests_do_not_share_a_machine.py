# === test_timing_tests_do_not_share_a_machine.py ======================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that the suite holding a timing measurement runs one test at a time.

`TreeNode.BuildingAFlatBusIsNotQuadratic` times a build at two sizes and compares them. The
comparison is the test: quadratic work quadruples when the count doubles, linear work doubles, and
the bound sits at 3. Taking the minimum of several runs throws away a hiccup inside one phase, and
not a change in load between the two phases, which is the quantity the ratio is made of.

It read 3.0499 against that bound on main (run 38120340775) as soon as the suite began running four
at a time, and 3.1942 in the nightly's flake hunt (run 38019530320), which has always run at every
processor. So its own suite carries RUN_SERIAL, which costs nothing: about 207 cases and 2.5 seconds
against 197 seconds of wall clock for the whole run.

Nothing fails when the property is dropped. The suite gets marginally faster and a merge fails
later, intermittently, on a test whose output reads like a performance regression in the code it
measures rather than like contention.

The lesson this encodes is narrower than "serialise timing tests" and worth keeping: raising the
parallel count has to be checked against the tests that measure time, and this one was diagnosed as
contention-sensitive hours before the same change put it under contention.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SUITE = ROOT / "components" / "term" / "test" / "CMakeLists.txt"
MEASURES_TIME = ROOT / "components" / "term" / "test" / "unit" / "tree_view_test.cpp"


def test_the_test_that_measures_time_is_still_there():
    """Fails if it is renamed or removed, which would leave the property below guarding nothing."""
    text = MEASURES_TIME.read_text(encoding="utf-8")
    assert "BuildingAFlatBusIsNotQuadratic" in text, f"the timing test is gone from {MEASURES_TIME.name}"
    assert "steady_clock" in text, "the test no longer measures time, so this check may be obsolete"


def test_its_suite_runs_one_test_at_a_time():
    """A ratio of two measured phases cannot share a machine with three other tests."""
    text = SUITE.read_text(encoding="utf-8")
    assert "unit/tree_view_test.cpp" in text, "tree_view_test.cpp has moved to another suite"
    assert re.search(r"EXTRA_PROPERTIES\s+RUN_SERIAL\s+TRUE", text), (
        "the suite holding the timing measurement does not carry RUN_SERIAL"
    )
