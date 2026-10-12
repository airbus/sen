# === test_a_generated_package_builds_against_the_install.py ===========================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that a leg installs Sen and builds a generated package against it.

Nothing else does. The archive check compares file names and `apps/cli_package/test` asserts the
generated `CMakeLists.txt` exists without reading it, which is how a moved config directory and a
stale template both reached main.

This guards the lane definition, not the behaviour. What it can catch is the step being dropped, or
its gate narrowing until no leg runs it -- a lane that stops running looks like a lane that passes.
"""

import re
from pathlib import Path

from generate_matrix_jobs import compute_jobs

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "standard_test.yaml"
STEP_NAME = "Build and run a generated package against the installed Sen"


def step() -> str:
    """The step's own block: from its name to the next step at the same indentation."""
    text = WORKFLOW.read_text(encoding="utf-8")
    start = text.find(f"- name: {STEP_NAME}")
    assert start != -1, f"no step named {STEP_NAME!r} in standard_test.yaml"
    rest = text[start:]
    following = re.search(r"\n      - name:", rest)
    return rest[: following.start()] if following else rest


def test_the_step_installs_generates_builds_and_runs():
    """The four claims the step exists to make, each needing the one before it."""
    block = step()
    assert "cmake --install" in block, "the step does not install Sen"
    assert "sen package init" in block, "the step does not generate a package with the real tool"
    assert "cmake --build" in block, "the step does not build the generated package"
    assert "--start-stop" in block, "the step does not start the generated package"


def test_the_consumer_is_given_the_prefix_without_a_suffix():
    """The config installs where CMake's search reaches it, which is the thing worth regressing on."""
    block = step()
    assert 'export SEN_PREFIX="$prefix"' in block, "the step does not hand the consumer the prefix"
    assert "$prefix/cmake" not in block, "the step appends the pre-move suffix to the prefix"


def test_some_leg_actually_runs_it():
    """The gate has to select a leg, or the step is present and never executes."""
    block = step()
    assert "matrix.check_package" in block and "runner.os == 'Linux'" in block, (
        "the step's gate is not the one this test reasons about"
    )
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    eligible = [j for j in jobs if j.check_package and j.runner.startswith("ubuntu")]
    assert eligible, "no Linux leg carries check_package, so the step never runs"
