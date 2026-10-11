# === test_compiler_cache_is_measurable.py =============================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that the legs building in the image publish their compiler cache statistics.

The `ccache-action` zeroes the counters and prints them, and it runs only for the legs that
build on the runner. The legs that build in the image restore and save the cache themselves,
so these two steps are the only reading they have. Without them a cap can be judged on a size
and never on a hit rate, which is the measurement that says whether it is too tight.

Nothing fails when the reading is missing. That is how it was lost the first time: the same
change that moved these legs off the action removed the cap and the statistics together, and
the legs went on reporting success with neither.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "standard_test.yaml"
TEXT = WORKFLOW.read_text(encoding="utf-8")

# A step begins at this indent; the next one bounds it.
STEP = re.compile(r"^      - ", re.M)


def steps() -> list[tuple[int, str]]:
    """Every step in the workflow, as (line number, text)."""
    starts = [match.start() for match in STEP.finditer(TEXT)]
    bounds = starts[1:] + [len(TEXT)]
    return [(TEXT.count("\n", 0, s) + 1, TEXT[s:e]) for s, e in zip(starts, bounds, strict=True)]


def steps_running(command: str) -> list[tuple[int, str]]:
    """The steps whose body carries this command."""
    return [(line, text) for line, text in steps() if command in text]


def test_the_counters_are_zeroed_and_read():
    """Either half alone is useless: unzeroed counters carry every past run, unread ones say nothing."""
    for command in ("ccache --zero-stats", "ccache --show-stats"):
        found = steps_running(command)
        assert len(found) == 1, f"{command} appears in {len(found)} steps, expected 1"
        line, text = found[0]
        assert "matrix.ci_image != ''" in text, f"{command} at line {line} is not scoped to the image legs"
        # On the runner the command would read an empty directory and report a cold cache,
        # which looks exactly like a working instrument on a leg that never hits.
        assert "in_image.sh" in text, f"{command} at line {line} runs outside the container"


def test_the_reading_comes_before_the_save():
    """Afterwards it would describe the cache the next run restores, not the run that wrote it."""
    read_at = steps_running("ccache --show-stats")[0][0]
    saves = [line for line, text in steps() if "actions/cache/save" in text and "~/.ccache" in text]
    assert saves, "no step saves the compiler cache"
    assert read_at < min(saves), f"the reading at line {read_at} follows the save at line {min(saves)}"
