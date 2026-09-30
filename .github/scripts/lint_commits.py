#!/usr/bin/env python3
# === lint_commits.py ==================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Lints the commit messages a branch adds, leaving the ones it only carries.

A squash merge lands the pull request description as the commit body on the target branch, and
prose wrapped for a web form runs well past the line limit. Porting such a commit to another branch
brings that body with it, and the gate then fails on a message that was already accepted where it
merged, which nobody can correct without rewriting a shared branch. So a commit whose patch already
exists on a mainline is carried, not authored, and is not this branch's message to judge.

`gitlint --commits <sha>` is not one commit: gitlint hands the argument to `git log`, which walks
the whole ancestry, so a single sha lints every commit behind it. Each commit is linted as its own
`<sha>~1..<sha>` range instead.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

DEFAULT_MAINLINES = ("origin/main",)


def git(*args: str, cwd: Path | None = None) -> str:
    """Runs git with the ambient repository variables cleared, so a fixture stays the fixture."""
    environment = {k: v for k, v in os.environ.items() if k not in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE")}
    return subprocess.run(
        ["git", *args],
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
        env=environment,
    ).stdout.strip()


def find_gitlint() -> str | None:
    """PATH first, then the environment pre-commit built, which is where a developer's copy lives."""
    found = shutil.which("gitlint")
    if found:
        return found
    cache = Path(os.environ.get("PRE_COMMIT_HOME", Path.home() / ".cache" / "pre-commit"))
    candidates = sorted(cache.glob("repo*/py_env-*/bin/gitlint"))
    return str(candidates[0]) if candidates else None


def carried_commits(mainlines: list[str], tip: str, cwd: Path | None = None) -> set[str]:
    """Commits whose patch already exists on a mainline, as `git cherry` marks them with a minus."""
    carried: set[str] = set()
    for mainline in mainlines:
        if not git("rev-parse", "--verify", "--quiet", mainline, cwd=cwd):
            continue
        for line in git("cherry", mainline, tip, cwd=cwd).splitlines():
            mark, _, sha = line.partition(" ")
            if mark == "-" and sha:
                carried.add(sha.strip())
    return carried


def main() -> int:
    """Lints the range and returns non-zero when a commit this branch wrote is at fault."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True, help="the branch this one will merge into")
    parser.add_argument("--tip", default="HEAD")
    parser.add_argument(
        "--mainline",
        action="append",
        default=None,
        help="a branch a commit may have been ported from; repeatable (default: origin/main)",
    )
    parser.add_argument("--quiet", action="store_true", help="print only failures")
    args = parser.parse_args()

    gitlint = find_gitlint()
    if not gitlint:
        print("no gitlint found; pip install gitlint or run pre-commit once", file=sys.stderr)
        return 2

    base = git("rev-parse", "--verify", "--quiet", args.base)
    if not base:
        print(f"lint_commits: {args.base} does not resolve", file=sys.stderr)
        return 2

    merge_base = git("merge-base", args.base, args.tip) or base
    commits = git("rev-list", "--reverse", f"{merge_base}..{args.tip}").split()
    mainlines = args.mainline if args.mainline is not None else list(DEFAULT_MAINLINES)
    carried = carried_commits(mainlines, args.tip)

    failed = []
    for sha in commits:
        subject = git("log", "-1", "--format=%s", sha)
        if sha in carried:
            if not args.quiet:
                print(f"  carried  {sha[:9]} {subject}")
            continue
        run = subprocess.run(
            [gitlint, "--commits", f"{sha}~1..{sha}"],
            capture_output=True,
            text=True,
            check=False,
        )
        if run.returncode == 0:
            if not args.quiet:
                print(f"  ok       {sha[:9]} {subject}")
        else:
            failed.append(sha)
            print(f"  FAILED   {sha[:9]} {subject}")
            for line in (run.stdout + run.stderr).splitlines():
                print(f"             {line}")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
