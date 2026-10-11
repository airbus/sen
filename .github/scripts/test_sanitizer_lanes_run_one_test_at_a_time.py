# === test_sanitizer_lanes_run_one_test_at_a_time.py ===================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that a sanitizer lane runs its suite one test at a time.

The rest of the repository runs four at once, which took ten minutes off the critical path. The
detectors are the exception, and not for speed: a detector shadows memory per process, so four of
them on a 4-processor runner with 15.6 GiB starves each one.

Measured rather than feared. At `--parallel 4` the TSan lane timed `InputTest.ASingleFlippedByteIs\
ReadOrRefused` out of its 20-second bound, which aborted the run before the flaky pass and left no
thread report archived at all [nightly run 38098378734]. The night before, serial, the same lane
ran 2859 tests with none failing in its main pass and archived 75 kB of race findings
[run 38019530320]. So the cost was not a slower lane, it was a lane that stopped measuring.

ASan was checked first and fits four processes comfortably, which is how this was nearly missed:
TSan's overhead is the larger of the two by a wide margin, and one detector is not evidence about
the other.

Nothing fails when the pin is dropped. The lane gets faster and reports less, and reporting less
is indistinguishable from having found nothing.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"
# A job is a sanitizer lane if its own body asks conan for a sanitizer.
SANITIZER = re.compile(r"sen/\*:sanitizer=")
PINNED = re.compile(r"'SEN_CTEST_JOBS':\s*'1'")
JOB = re.compile(r"^  (?P<name>[a-z][a-z0-9-]*):$", re.M)


def sanitizer_lanes() -> list[tuple[Path, str, str]]:
    """Every job whose body turns a sanitizer on, as (workflow, job name, body).

    Scoped per job rather than per conan invocation: a workflow holds lanes that are nothing to
    do with the sanitizers, and asking whether the pin appears anywhere in the file would pass
    them all on one lane's behalf.
    """
    found = []
    for workflow in sorted(WORKFLOWS.glob("*.yaml")):
        text = workflow.read_text(encoding="utf-8")
        starts = [(m.group("name"), m.start()) for m in JOB.finditer(text)]
        for index, (name, offset) in enumerate(starts):
            end = starts[index + 1][1] if index + 1 < len(starts) else len(text)
            body = text[offset:end]
            if SANITIZER.search(body):
                found.append((workflow, name, body))
    return found


def test_there_are_sanitizer_lanes_to_check():
    """Fails if the lanes are renamed or restructured, leaving the pin below unverified."""
    lanes = sanitizer_lanes()
    assert lanes, "no job asks conan for a sanitizer"
    names = sorted(f"{w.name}:{n}" for w, n, _ in lanes)
    assert names == ["nightly.yaml:sanitizers", "pr_sanitizers.yaml:sanitizers"], names


def test_every_sanitizer_lane_pins_one_test_at_a_time():
    """A detector that shares a runner with three of itself reports less, not slower."""
    unpinned = [f"{w.name}:{n}" for w, n, body in sanitizer_lanes() if not PINNED.search(body)]
    assert not unpinned, f"sanitizer lanes not pinned to one test at a time: {unpinned}"
