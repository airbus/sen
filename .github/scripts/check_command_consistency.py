# === check_command_consistency.py =====================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Fails when a command printed in prose is one that will not work for a reader.

The documentation, the README, CONTRIBUTING and the example guides all print
commands people copy. Nothing runs them, so they rot in a way tests do not catch:
CI builds Sen its own way and never executes the line a newcomer pastes.

That gap produced F-229. Every `conan install` in the corpus said `--profile`, which
sets only the host profile and leaves Conan looking for a build profile named
`default`. CI creates a `default` before building, so the pipeline was green while the
documented command was wrong for every reader who did not have one. The sweep that
fixed it reached `docs/`, `README.md` and `examples/README.md`, and missed
`CONTRIBUTING.md`, which stayed wrong until somebody read it.

Each rule below is an invariant a command in prose has to satisfy. They are deliberately
narrow: a rule fires on an actual invocation, never on prose *about* a flag, so pages
can still explain why `--profile` is the wrong one to use.

    python3 .github/scripts/check_command_consistency.py           # whole repository
    python3 .github/scripts/check_command_consistency.py FILE ...  # named files
"""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

# Directories whose markdown is not ours to lint, or is generated.
SKIPPED = ("node_modules", "build", ".git", "dist", "doxygen_gen", "docs/snippets", "licenses")


@dataclass(frozen=True)
class Rule:
    """One invariant, and what to say when a line breaks it."""

    name: str
    trigger: re.Pattern[str]
    accept: re.Pattern[str] | None
    message: str


RULES = (
    Rule(
        name="conan-profile-all",
        # An actual invocation: `conan` then a verb that takes a profile.
        trigger=re.compile(r"\bconan\s+(?:install|build|create|lock|graph|profile\s+show)\b[^\n`]*--profile"),
        # `--profile:all`, `--profile:host`, `--profile:build` are all explicit. `-pr:a` too.
        accept=re.compile(r"--profile:(?:all|host|build)\b"),
        message=(
            "uses bare `--profile`, which sets only the host profile. A reader without a "
            "`default` build profile gets an error, and a reader with one silently builds "
            "for their own machine. Use `--profile:all` (or `-pr:a`). See F-229."
        ),
    ),
    Rule(
        name="installer-version",
        trigger=re.compile(r"install\.sh[^\n`]*\ssh\s+-s\s+--\s+(\d+\.\d+\.\d+)"),
        accept=None,  # handled by the cross-file check below
        message="pins an installer version; all of them have to agree.",
    ),
)


def markdown_files(paths: list[str]) -> list[Path]:
    """The markdown to lint: the named files, or the whole repository."""
    if paths:
        return [Path(p) for p in paths]
    out = []
    for path in sorted(REPO_ROOT.rglob("*.md")):
        rel = path.relative_to(REPO_ROOT).as_posix()
        if any(skip in rel for skip in SKIPPED):
            continue
        out.append(path)
    return out


def display_path(path: Path) -> str:
    """A path as it should be reported, whether the caller gave us an absolute one or not.

    Both entry points need this. When it lived in only one of them, the other
    crashed with `ValueError: not in the subpath of` on any relative argument.
    """
    if not path.is_absolute():
        return path.as_posix()
    try:
        return path.relative_to(REPO_ROOT).as_posix()
    except ValueError:
        # Outside the repository: report it whole rather than dying on the way to saying so.
        return path.as_posix()


def check_line(rule: Rule, line: str) -> bool:
    """True when the line triggers the rule and does not satisfy it."""
    if not rule.trigger.search(line):
        return False
    if rule.accept is None:
        return False
    return not rule.accept.search(line)


def collect_versions(files: list[Path]) -> dict[str, list[str]]:
    """Installer versions pinned in prose, grouped by version."""
    rule = RULES[1]
    found: dict[str, list[str]] = {}
    for path in files:
        for number, line in enumerate(path.read_text(errors="ignore").splitlines(), 1):
            match = rule.trigger.search(line)
            if match:
                where = f"{display_path(path)}:{number}"
                found.setdefault(match.group(1), []).append(where)
    return found


def main() -> int:
    """Lint every command in prose, and report each line that a rule rejects."""
    files = markdown_files(sys.argv[1:])
    problems = []

    for path in files:
        rel = display_path(path)
        for number, line in enumerate(path.read_text(errors="ignore").splitlines(), 1):
            for rule in RULES:
                if check_line(rule, line):
                    problems.append(f"{rel}:{number}: {rule.message}")

    versions = collect_versions(files)
    if len(versions) > 1:
        listed = ", ".join(f"{version} ({len(sites)}x)" for version, sites in sorted(versions.items()))
        problems.append(f"installer version disagrees across the corpus: {listed}")
        for version, sites in sorted(versions.items()):
            for site in sites:
                problems.append(f"  {site}: pins {version}")

    for problem in problems:
        print(problem)

    print(f"\n{len(problems)} problem(s) in {len(files)} file(s).")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
