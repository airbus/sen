# === test_create_changelog.py =========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the grouping and breaking-change extraction of the changelog."""

import configparser
import subprocess
from pathlib import Path

from create_changelog import TYPE_MAP, build_changelog, get_commit_range, get_last_release_branch

GITLINT_FILE = Path(__file__).resolve().parents[2] / ".gitlint"


def git(repo: Path, *args: str) -> None:
    """Runs a git command in the test repository."""
    subprocess.run(["git", "-C", str(repo), *args], check=True, capture_output=True)


def commit(repo: Path, subject: str) -> None:
    """Adds one commit to the test repository."""
    (repo / "file.txt").write_text(subject)
    git(repo, "add", "file.txt")
    git(repo, "-c", "commit.gpgsign=false", "commit", "-qm", subject)


def test_commits_are_grouped_by_type():
    """Each conventional type gets its own section, in TYPE_MAP order."""
    text = build_changelog(["fix: repair the flux\n\n", "feat: add the flux\n\n", "feat: tune the flux\n\n"])
    features = text.index("### ✨ Features")
    fixes = text.index("### 🐛 Bug Fixes")
    assert features < fixes
    assert "- feat: add the flux" in text
    assert "- feat: tune the flux" in text
    assert "- fix: repair the flux" in text


def test_breaking_marker_in_title_is_reported():
    """A `!` after the type lands in the breaking-changes section."""
    text = build_changelog(["feat!: drop the old api\n\n"])
    assert "### 💥 BREAKING CHANGES" in text
    assert "- **feat!: drop the old api**" in text


def test_breaking_change_footer_supplies_the_description():
    """A BREAKING CHANGE footer is quoted under the entry."""
    text = build_changelog(["fix: change defaults\nBREAKING CHANGE: defaults are now strict\n"])
    assert "  > defaults are now strict" in text


def test_a_wrapped_breaking_description_is_kept_whole():
    """Messages wrap at 80 columns, so the description is usually more than one line."""
    text = build_changelog(
        [
            "feat!: retire the env interface\nBREAKING CHANGE: the first sentence.\nAnd the rest of it.\n\n"
            "Not the body paragraph after the blank line.\n"
        ]
    )
    assert "  > the first sentence. And the rest of it." in text
    assert "Not the body paragraph" not in text.split("### ")[1]


def test_scoped_and_unknown_titles():
    """Scoped titles group under their type; unparsed ones are listed, not dropped."""
    text = build_changelog(["chore[deps]: bump urllib3\n\n", "merge remote tracking branch\n\n"])
    assert "- chore[deps]: bump urllib3" in text
    assert "- merge remote tracking branch" in text


def test_empty_input_renders_the_header_only():
    """No commits still renders a valid, header-only changelog."""
    assert build_changelog([]).startswith("## 📝 Change Log")


def test_scoped_subjects_are_grouped_like_any_other():
    """This repository writes scopes in parentheses, which gitlint enforces."""
    text = build_changelog(["feat(installer): add the installer\n\n", "fix(core,kernel): repair the flux\n\n"])
    assert "- feat(installer): add the installer" in text
    assert "- fix(core,kernel): repair the flux" in text


def test_scoped_breaking_marker_is_reported():
    """A scope must not hide the ! marker; that is the commit people need to see."""
    text = build_changelog(["feat(gen)!: drop the old options\n\n"])
    assert "### 💥 BREAKING CHANGES" in text
    assert "- **feat(gen)!: drop the old options**" in text


def test_unparsed_subjects_are_listed_rather_than_dropped():
    """A subject that does not parse still belongs in the notes."""
    text = build_changelog(["Merge branch 'main' into topic\n\n"])
    assert "Merge branch 'main' into topic" in text


def test_every_type_gitlint_accepts_has_a_section():
    """A type that can be committed but has no section here would be dropped from the notes."""
    config = configparser.ConfigParser()
    config.read(GITLINT_FILE)
    accepted = config["contrib-title-conventional-commits"]["types"].split(",")
    assert sorted(accepted) == sorted(TYPE_MAP)


def test_the_previous_release_line_is_chosen_from_a_detached_head(tmp_path, monkeypatch):
    """A release runs on a tag, so the line being released has no branch name to compare against."""
    repo = tmp_path / "repo"
    repo.mkdir()
    git(repo, "init", "-q", "-b", "main")
    git(repo, "config", "user.email", "test@example.com")
    git(repo, "config", "user.name", "Test")
    commit(repo, "feat: the 0.5 work")
    git(repo, "branch", "release/0.5.x")
    commit(repo, "feat: the 0.6 work")
    git(repo, "branch", "release/0.6.x")
    git(repo, "checkout", "-q", "--detach", "release/0.6.x")
    monkeypatch.chdir(repo)

    # Not release/0.6.x: that is the line being released, and its merge base with HEAD is HEAD,
    # which would leave the release with an empty changelog.
    assert get_last_release_branch() == "release/0.5.x"
    assert get_commit_range() != "HEAD"


def test_a_type_without_a_section_is_listed_rather_than_dropped():
    """The sections are driven by TYPE_MAP, so an unmapped type has to fall back to Other."""
    text = build_changelog(["wip: half of a thing\n\n"])
    assert "### 🧾 Other" in text
    assert "- wip: half of a thing" in text
