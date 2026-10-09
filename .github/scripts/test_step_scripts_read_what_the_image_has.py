# === test_step_scripts_read_what_the_image_has.py =====================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks what a step's script may read when it runs inside the CI image.

in_image.sh forwards a fixed list and nothing else: a step's own `env:` reaches the runner but not
the container. These scripts run under `set -u`, so reading a name outside that list ends the step
before its first command, on every leg that uses the image, whatever the name was wanted for.

That is how GITHUB_WORKSPACE, read one line above the test deciding whether it was wanted, stopped
the Linux legs on 2026-10-08 while the Windows leg it was for ran fine.
"""

import re
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
WORKFLOWS = ROOT / ".github" / "workflows"
IN_IMAGE = ROOT / ".github" / "scripts" / "in_image.sh"

# Set by the shell itself, so a script may read them wherever it runs.
ALWAYS = {"PWD", "OLDPWD", "PATH", "HOME", "SHELL", "USER", "TERM", "IFS", "RANDOM", "LINENO"}
POSITIONAL = re.compile(r"^(\d+|[@*?$!#_-])$")
REFERENCE = re.compile(r"\$\{?([A-Za-z_][A-Za-z0-9_]*)\}?")
GUARDED = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*):?[-?+=]")
ASSIGNED = re.compile(r"(?:^|[;&|(\s])([A-Za-z_][A-Za-z0-9_]*)=", re.MULTILINE)
BOUND_BY = re.compile(r"\b(?:for|read|mapfile|readarray)\s+(?:-[A-Za-z]+\s+)*([A-Za-z_][A-Za-z0-9_]*)")

# Set by a file an earlier step generates and this one sources, which no reading of the script
# can see. Listed rather than skipping every script that sources something: the step that lost
# the Linux legs sources conanbuild.sh, and skipping it would have missed exactly that.
FROM_A_SOURCED_FILE = {
    # .cache/clang-tidy-helpers, written by "Find the clang-tidy helpers" in clang_tidy_diff.yaml.
    "CLANG_TIDY_DIFF",
    "RUN_CLANG_TIDY",
}
EXPRESSION = re.compile(r"\$\{\{.*?\}\}", re.S)
HEREDOC = re.compile(r"<<'IN'\n(.*?)\n\s*IN\b", re.S)


def forwarded() -> set[str]:
    """The names in_image.sh passes through, read from the script rather than copied here."""
    names = set(re.findall(r"--env\s+([A-Za-z_][A-Za-z0-9_]*)", IN_IMAGE.read_text(encoding="utf-8")))
    assert names, "no --env forwarding found; the script moved and this check reads nothing"
    return names


def scripts_in_the_image() -> list[tuple[str, str, str]]:
    """(workflow, step, body) for every step piping a heredoc into the image."""
    found = []
    for path in sorted(WORKFLOWS.glob("*.yaml")):
        jobs = (yaml.safe_load(path.read_text(encoding="utf-8")) or {}).get("jobs") or {}
        for job in jobs.values():
            for step in (job or {}).get("steps") or []:
                run = (step or {}).get("run") or ""
                if "in_environment.sh <<" not in run and "in_image.sh <<" not in run:
                    continue
                for body in HEREDOC.findall(run):
                    found.append((path.name, step.get("name", "?"), body))
    return found


def unguarded(body: str) -> set[str]:
    """Names the script reads without a default and does not set for itself."""
    # GitHub substitutes these before bash sees them, so they are not shell references.
    body = EXPRESSION.sub("x", body)
    known = set(ASSIGNED.findall(body)) | set(BOUND_BY.findall(body)) | set(GUARDED.findall(body))
    read = {name for name in REFERENCE.findall(body) if not POSITIONAL.match(name)}
    return read - known - ALWAYS - FROM_A_SOURCED_FILE - forwarded()


def test_the_scan_finds_the_steps():
    """An empty scan would pass the case below without reading a single script."""
    assert len(scripts_in_the_image()) >= 10


def test_no_step_reads_a_name_the_image_was_not_given():
    """Set -u turns a name the container never received into a step that runs nothing."""
    problems = []
    for workflow, step, body in scripts_in_the_image():
        names = unguarded(body)
        if names:
            problems.append(f"{workflow} / {step}: {', '.join(sorted(names))}")
    assert not problems, "read without a default and not forwarded:\n  " + "\n  ".join(problems)
