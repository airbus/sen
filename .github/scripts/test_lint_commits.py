#!/usr/bin/env python3
# === test_lint_commits.py =============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that the commit lint judges what a branch writes and carries what it only ported.

The fixture builds the shape that produced this: a mainline commit whose body is web-form prose,
too wide for the limit, ported onto a branch that also has a badly written commit of its own. The
port has to pass and the branch's own commit has to fail, and the same body has to fail when it is
the branch's own work -- otherwise the rule is not distinguishing authorship, only forgiving width.
"""

import os
import subprocess
import sys
from pathlib import Path

import pytest

SCRIPT = Path(__file__).resolve().parent / "lint_commits.py"
sys.path.insert(0, str(SCRIPT.parent))

from lint_commits import find_gitlint  # noqa: E402

WIDE_BODY = "This is the pull request description as a squash merge lands it, " + "wrapped for a web form " * 8
CLEAN_BODY = "A body, because gitlint asks for one."

pytestmark = pytest.mark.skipif(find_gitlint() is None, reason="gitlint is not installed here")


def run_git(repo: Path, *args: str) -> None:
    """Runs git in the fixture, with the ambient repository variables cleared."""
    environment = {k: v for k, v in os.environ.items() if k not in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE")}
    subprocess.run(["git", *args], cwd=repo, check=True, capture_output=True, text=True, env=environment)


def commit(repo: Path, filename: str, subject: str, body: str) -> None:
    """Adds one commit, bypassing this repository's own hooks."""
    (repo / filename).write_text(filename, encoding="utf-8")
    run_git(repo, "add", filename)
    run_git(repo, "commit", "-m", subject, "-m", body, "--no-verify")


@pytest.fixture
def repo(tmp_path: Path) -> Path:
    """A mainline with a wide-bodied commit, and a branch that ported it and wrote one of its own.

    The branch commits first. Without that its cherry-pick lands on the same parent with the same
    tree, message and timestamps, and git hands back the identical sha -- one commit on two branch
    names, an empty range, and a test that asserts nothing.
    """
    path = tmp_path / "fixture"
    path.mkdir()
    run_git(path, "init", "-q", "-b", "mainline")
    run_git(path, "config", "user.email", "fixture@example.com")
    run_git(path, "config", "user.name", "Fixture")
    commit(path, "base.txt", "chore: the commit both branches share", CLEAN_BODY)
    run_git(path, "branch", "feature")

    commit(path, "ported.txt", "test: a fix that merged by squash", WIDE_BODY)
    ported = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=path, capture_output=True, text=True, check=True
    ).stdout.strip()

    run_git(path, "switch", "-q", "feature")
    commit(path, "branch_work.txt", "feat: what this branch is for", CLEAN_BODY)
    run_git(path, "cherry-pick", ported)
    return path


def lint(repo: Path, *extra: str) -> subprocess.CompletedProcess:
    """Runs the helper against the fixture with `mainline` as the base."""
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--base", "mainline", *extra],
        cwd=repo,
        capture_output=True,
        text=True,
        check=False,
    )


def test_a_ported_commit_is_carried_not_judged(repo):
    """The case that blocked a shared branch: a body that is only wide because it merged elsewhere."""
    run = lint(repo, "--mainline", "mainline")
    assert run.returncode == 0, run.stdout + run.stderr
    assert "carried" in run.stdout, run.stdout


def test_the_same_body_fails_when_the_branch_wrote_it(repo):
    """Without this the rule would just be forgiving wide bodies wherever they came from."""
    commit(repo, "own.txt", "test: a fix written on this branch", WIDE_BODY)
    run = lint(repo, "--mainline", "mainline")
    assert run.returncode == 1, run.stdout + run.stderr
    assert "a fix written on this branch" in run.stdout, run.stdout
    assert "carried" in run.stdout, "the port must still be carried while the authored one fails"


def test_without_the_mainline_the_port_is_judged_too(repo):
    """The control: pointing the port check at nothing must bring the failure back."""
    run = lint(repo, "--mainline", "refs/heads/does-not-exist")
    assert run.returncode == 1, run.stdout + run.stderr
    assert "carried" not in run.stdout, run.stdout


def test_a_well_written_commit_of_its_own_passes(repo):
    """The ordinary case, so the rule is not passing everything for the wrong reason."""
    commit(repo, "good.txt", "fix: something worth a line", CLEAN_BODY)
    run = lint(repo, "--mainline", "mainline")
    assert run.returncode == 0, run.stdout + run.stderr


def test_an_unresolvable_base_is_an_error_not_a_pass(repo):
    """A gate that cannot find its base must not report success."""
    run = subprocess.run(
        [sys.executable, str(SCRIPT), "--base", "refs/heads/nowhere"],
        cwd=repo,
        capture_output=True,
        text=True,
        check=False,
    )
    assert run.returncode == 2, run.stdout + run.stderr


def test_quiet_prints_only_failures(repo):
    """The pre-push hook shows this output, where fifteen ok lines would bury the one failure."""
    commit(repo, "own.txt", "test: a fix written on this branch", WIDE_BODY)
    run = lint(repo, "--mainline", "mainline", "--quiet")
    assert run.returncode == 1
    assert "carried" not in run.stdout
    assert "FAILED" in run.stdout
