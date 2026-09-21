"""Checks that a caller grants at least what the workflow it calls asks for.

A reusable workflow cannot hold more than its caller grants. The mismatch is not a failing
job: the run ends in startup_failure with no jobs and no log, which is why main.yaml carries
the warning in five places. It happened anyway on 2026-09-21, to the lane whose call site
holds the warning.
"""

from pathlib import Path

import pytest
import yaml

ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"
# read is implied by write, so a caller granting write satisfies a called read.
RANK = {"none": 0, "read": 1, "write": 2}


def callers() -> list[tuple[Path, str, dict, Path]]:
    """Returns (caller, job name, granted permissions, called workflow) for every local call."""
    found = []
    for path in sorted(WORKFLOWS.glob("*.yaml")):
        jobs = (yaml.safe_load(path.read_text(encoding="utf-8")) or {}).get("jobs") or {}
        for name, job in jobs.items():
            uses = (job or {}).get("uses", "")
            if not uses.startswith("./.github/workflows/"):
                continue
            found.append((path, name, (job or {}).get("permissions") or {}, ROOT / uses[2:]))
    return found


def asked_for(called: Path) -> dict:
    """The union of what every job in the called workflow requests."""
    doc = yaml.safe_load(called.read_text(encoding="utf-8")) or {}
    wanted = dict(doc.get("permissions") or {})
    for job in (doc.get("jobs") or {}).values():
        for scope, level in ((job or {}).get("permissions") or {}).items():
            if RANK.get(level, 0) > RANK.get(wanted.get(scope, "none"), 0):
                wanted[scope] = level
    return wanted


@pytest.mark.parametrize(
    "caller,job,granted,called", callers(), ids=lambda v: v.name if isinstance(v, Path) else str(v)
)
def test_the_caller_grants_what_the_called_workflow_asks(caller, job, granted, called):
    """One case per call site, so a failure names the job that would not start."""
    short = []
    for scope, level in asked_for(called).items():
        if RANK.get(level, 0) > RANK.get(granted.get(scope, "none"), 0):
            short.append(f"{scope}: {called.name} asks {level}, {caller.name} grants {granted.get(scope, 'none')}")
    assert not short, f"job '{job}' would end in startup_failure -- " + "; ".join(short)


def test_there_is_something_to_check():
    """Without this an empty scan would pass every case above."""
    assert len(callers()) >= 5
