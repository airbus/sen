# === test_compiler_cache_keys_agree.py ================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that every place standard_test.yaml spells the compiler cache key spells it the same.

Four steps build it: restore, the ccache action for the legs outside the image, the drop that
clears the entry a save would be refused for, and the save. A cache key is immutable, so a drop
that misses means the save is refused and the entry never refreshes again. Nothing fails when
that happens: the lane stays green and quietly compiles from cold.
"""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "standard_test.yaml"

# The key starts with the tool, which differs by platform, so the comparison starts after it.
FROM = "matrix.runner"


def key_sites() -> list[tuple[int, str]]:
    """Every line that builds a compiler cache key, as (line number, key after the tool name)."""
    found = []
    for number, line in enumerate(WORKFLOW.read_text(encoding="utf-8").splitlines(), 1):
        if "matrix.build_type" not in line:
            continue
        if "key:" in line or "drop_cache_key.sh" in line:
            found.append((number, line.split(FROM, 1)[1].strip().rstrip('"')))
    return found


def test_the_four_steps_name_the_same_key() -> None:
    """Restore, the action, the drop and the save, or a save is refused and never refreshes."""
    sites = key_sites()
    assert len(sites) == 4, [number for number, _ in sites]
    assert len({key for _, key in sites}) == 1, sites


def test_the_key_carries_whether_the_leg_instruments() -> None:
    """Instrumented objects are not interchangeable with plain ones, so they cannot share a key."""
    for number, key in key_sites():
        assert "enable_coverage" in key, f"line {number} would share one key with both builds"
