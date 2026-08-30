# === test_lanes.py ====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks every lane definition against what its workflow actually runs.

lanes.py exists so that a developer runs what CI runs, which holds only while the two
agree. A definition that had drifted would answer confidently and wrongly.

The comparison is literal, token for token. Anything in a modelled job that is not a
lane command has to be named below, so a step added to a workflow fails here rather
than being left out of the local run.
"""

import dataclasses
import re
import shlex

import lanes
import pytest
import yaml

WORKFLOWS = lanes.ROOT / ".github" / "workflows"

# Run lines that are not part of a lane, matched whole rather than by their first word:
# a step that installed something a lane needed would otherwise be excused by looking
# like one that does not.
NOT_LANE_COMMANDS = {
    # The runner installs what the image already carries.
    ("pip", "install", "junitparser==5.0.1"),
    # Writes a cache key into the job environment. There is no cache to key locally.
    ("echo", "cache_date=$(/bin/date -u +%Y%m%d)", ">>", "$GITHUB_ENV"),
}

EXPRESSION = re.compile(r"\$\{\{([^}]*)\}\}")
ASSIGNMENT = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)=(.*)$")


def run_lines(workflow: str, job: str) -> list[str]:
    """Every line of every run step of one job, continuations joined, comments dropped."""
    document = yaml.safe_load((WORKFLOWS / workflow).read_text(encoding="utf-8"))
    jobs = document["jobs"]
    assert job in jobs, f"{workflow} has no job {job!r}; a lane names it, so one of the two was renamed"
    body = "\n".join(step["run"] for step in jobs[job]["steps"] if "run" in step)
    body = body.replace("\\\n", " ")
    return [line.strip() for line in body.splitlines() if line.strip() and not line.strip().startswith("#")]


def substitute(line: str, values: dict[str, str]) -> str:
    """Resolves the workflow expressions in one line, refusing any it was not given.

    An unknown expression would otherwise be compared as literal text.
    """

    def replace(match: re.Match) -> str:
        name = match.group(1).strip()
        if name not in values:
            raise AssertionError(f"no value for the workflow expression {name!r} in: {line}")
        return values[name]

    return EXPRESSION.sub(replace, line)


def expand(lines: list[str]) -> list[str]:
    """Resolves the shell variables a step assigns to itself, and drops the assignments."""
    assigned: dict[str, str] = {}
    expanded = []
    for line in lines:
        match = ASSIGNMENT.match(line)
        if match and not line.startswith("export "):
            name, raw = match.groups()
            # A whole-line assignment quotes its value; shlex gives the value back.
            assigned[name] = shlex.split(raw)[0] if raw else ""
            continue
        resolved = line
        for name, value in assigned.items():
            resolved = resolved.replace(f"${name}", value)
        expanded.append(resolved)
    return expanded


def workflow_commands(lane: lanes.Lane) -> list[list[str]]:
    """The lane commands the workflow runs, as argument lists.

    Every line that is not one has to be accounted for by NOT_LANE_COMMANDS, so a new
    step cannot go unnoticed by being unrecognised.
    """
    # The nightly runs both sanitizer lanes from one job, so the lane picks the leg.
    sanitizer = [option for option in lane.options if option.startswith("sen/*:sanitizer=")]
    values = {"matrix.sanitizer": sanitizer[0].split("=", 1)[1]} if sanitizer else {}

    commands = []
    for line in expand(run_lines(lane.workflow, lane.job)):
        tokens = shlex.split(substitute(line, values))
        if not tokens:
            continue
        if tuple(tokens) in NOT_LANE_COMMANDS:
            continue
        assert tokens[0] in ("conan", "cmake", "source", "python3"), (
            f"{lane.workflow}:{lane.job} runs {tokens[0]!r}, which lanes.py does not model "
            f"and NOT_LANE_COMMANDS does not excuse: {line}"
        )
        commands.append(tokens)
    return commands


@pytest.mark.parametrize("lane", lanes.LANES, ids=lambda lane: lane.name)
def test_the_lane_runs_what_its_workflow_runs(lane):
    """A lane definition and the job it names have to be the same commands."""
    assert lanes.commands(lane) == workflow_commands(lane)


@pytest.mark.parametrize("lane", lanes.LANES, ids=lambda lane: lane.name)
def test_the_job_the_lane_names_exists(lane):
    """Without this, a renamed job would empty the comparison above rather than fail it."""
    document = yaml.safe_load((WORKFLOWS / lane.workflow).read_text(encoding="utf-8"))
    assert lane.job in document["jobs"], f"{lane.workflow} has no job {lane.job!r}"
    assert workflow_commands(lane), f"{lane.workflow}:{lane.job} contributed no commands to compare"


@pytest.mark.parametrize("lane", lanes.LANES, ids=lambda lane: lane.name)
def test_the_lane_uses_the_compiler_its_job_sets(lane):
    """The compiler version reaches the commands nowhere, so compare it separately.

    build/<compiler>/<build_type> carries the name and not the version, so a lane
    naming the wrong version would generate commands that match and then build with a
    compiler CI does not use.
    """
    document = yaml.safe_load((WORKFLOWS / lane.workflow).read_text(encoding="utf-8"))
    environment = document["jobs"][lane.job].get("env", {})
    assert (environment.get("CC"), environment.get("CXX")) == (lane.cc, lane.cxx)


def test_the_comparison_can_fail():
    """Without this, a comparison that always succeeded would read as agreement."""
    lane = lanes.BY_NAME["coverage"]
    drifted = dataclasses.replace(lane, target="run_tests")
    assert lanes.commands(drifted) != workflow_commands(lane)


def test_no_two_lanes_are_the_same_lane():
    """A duplicate name loses a lane in BY_NAME, and duplicate commands hide a mistake."""
    for index, lane in enumerate(lanes.LANES):
        for other in lanes.LANES[index + 1 :]:
            assert lane.name != other.name
            assert lanes.commands(lane) != lanes.commands(other), f"{lane.name} and {other.name} are one lane"
