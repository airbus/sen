# === test_the_squash_subject_lint_reads_the_title.py ==================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that the lint of the squash subject reads the pull request title.

The repository squashes under the title whatever the commit count
(`squash_merge_commit_title` is `PR_TITLE`), so a lint that reads a lone commit's subject
instead passes a title the merge queue then rejects. #597 was ejected that way: its commit
subject was 61 characters and passed here, while the title landed 78 against gitlint's 72.
"""

from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "pr_title.yaml"


def lint_step() -> dict:
    """The one step that pipes a subject into gitlint."""
    jobs = (yaml.safe_load(WORKFLOW.read_text(encoding="utf-8")) or {})["jobs"]
    piping = [s for s in jobs["lint-title"]["steps"] if "| gitlint" in (s.get("run") or "")]
    assert len(piping) == 1, f"expected one step piping into gitlint, found {len(piping)}"
    return piping[0]


def test_the_subject_linted_is_the_title() -> None:
    """The title is the subject a squash lands, so the title is what the step has to read."""
    step = lint_step()
    assert "PR_TITLE" in (step.get("env") or {}), "the step must take the title from the event"
    assert '"$PR_TITLE" "$PR_NUMBER" | gitlint' in step["run"], "the title must be what gitlint reads"


def test_the_commit_count_does_not_choose_the_subject() -> None:
    """A one-commit branch still squashes under the title, so no count may pick another subject."""
    run = lint_step()["run"]
    assert "rev-list --count" not in run, "the commit count must not choose the subject"
    assert "git log -1 --format=%s" not in run, "a commit subject is not what the squash lands"
