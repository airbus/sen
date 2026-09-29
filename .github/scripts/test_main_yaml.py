# === test_main_yaml.py ================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Guard the ci-ok aggregator in main.yaml.

ci-ok is the only required status check on main, and it only blocks what it
`needs`. A job missing from that list can fail without blocking a merge, so
every job must appear there.
"""

from pathlib import Path

import yaml

MAIN_YAML = Path(__file__).resolve().parents[1] / "workflows" / "main.yaml"


def test_ci_ok_needs_every_job():
    """Every job in main.yaml must appear in the needs list of ci-ok."""
    jobs = yaml.safe_load(MAIN_YAML.read_text())["jobs"]
    needs = set(jobs["ci-ok"]["needs"])
    others = set(jobs) - {"ci-ok"}
    missing = sorted(others - needs)
    stale = sorted(needs - others)
    assert needs == others, f"ci-ok.needs is out of sync with the job list: missing {missing}, stale {stale}"
