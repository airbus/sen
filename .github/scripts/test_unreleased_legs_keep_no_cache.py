# === test_unreleased_legs_keep_no_cache.py ============================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that the nightly arm lane stores no compiler cache.

arm left the standard-test matrix because no release publishes it, and the reason that was worth
doing is the cache budget: its entry was 1012 MB of a 10 GiB repository limit that cannot be
raised. A scheduled run is on main, and every compiler-cache save in this repository is gated on
main -- so a lane carried over with its caching intact would go on writing the entry the move
exists to stop writing, and the change would free nothing.

Nothing fails when that happens. The lane is green either way and the budget stays full, which is
the same shape as the defect that prompted all of this: a cache save refused on every run while the
step reported success.
"""

import re
from pathlib import Path

from generate_matrix_jobs import compute_jobs

ROOT = Path(__file__).resolve().parents[2]
NIGHTLY = ROOT / ".github" / "workflows" / "nightly.yaml"
TEXT = NIGHTLY.read_text(encoding="utf-8")

# A job begins at this indent; the next one bounds it.
JOB = re.compile(r"^  (?P<name>[a-z][a-z0-9-]*):$", re.M)


def job(name: str) -> str:
    """One job's text, from its key to the next job's."""
    starts = [(m.group("name"), m.start()) for m in JOB.finditer(TEXT)]
    names = [n for n, _ in starts]
    assert name in names, f"no {name} job in nightly.yaml; it has {names}"
    index = names.index(name)
    end = starts[index + 1][1] if index + 1 < len(starts) else len(TEXT)
    return TEXT[starts[index][1] : end]


def test_the_arm_lane_exists_and_runs_on_arm():
    """Fails if the lane is renamed or dropped, which would leave the rest of this guarding nothing."""
    lane = job("arm")
    assert "runs-on: ubuntu-24.04-arm" in lane, "the arm lane does not run on an arm runner"
    assert "ctest" in lane, "the arm lane does not run the suite"


def test_the_arm_lane_stores_no_compiler_cache():
    """A save here writes the entry the matrix change exists to stop writing."""
    lane = job("arm")
    for marker in ("actions/cache", "ccache-action", "sccache"):
        assert marker not in lane, f"the arm lane carries {marker}, so it would keep its cache"


def test_arm_is_not_in_the_standard_test_matrix():
    """The other half: this lane replaces the matrix leg rather than duplicating it.

    Read from the computed matrix, not from the generator's text: `ubuntu-24.04-arm` still appears
    there as a permitted value of the runner field, which is a declaration and not a job.
    """
    for target_main in (False, True):
        jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=target_main)
        on_arm = [job.name for job in jobs if job.arch == "arm"]
        assert not on_arm, f"arm is in the matrix as well as the nightly: {on_arm}"
