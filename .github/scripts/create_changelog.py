# === create_changelog.py ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Builds the release changelog from the conventional commit messages.

The commit range starts at the merge base with the previous release branch;
without one, every commit reachable from HEAD is included. The output is
the body of the draft GitHub release.
"""

import re
import subprocess
import sys
from collections import defaultdict

# One entry per type that .gitlint accepts, so every commit that can land has a
# section to land in. test_create_changelog.py checks the two lists still match.
TYPE_MAP = {
    "feat": "✨ Features",
    "fix": "🐛 Bug Fixes",
    "build": "🏗️ Build System",
    "ci": "👷 Continuous Integration",
    "docs": "📚 Documentation",
    "refactor": "🔨 Refactor",
    "perf": "🚀 Performance",
    "test": "🚨 Tests",
    "revert": "⏪ Reverts",
    "chore": "📦 Chores",
}

# Subjects that do not parse, and those whose type has no section, still belong in
# the notes: dropping them silently is how a release ships with commits nobody listed.
UNPARSED_SECTION = "🧾 Other"

TITLE_PATTERN = re.compile(r"^(?P<type>\w+)(?:[(\[](?P<scope>[^)\]]+)[)\]])?(?P<breaking>!)?: (?P<subject>.+)")

COMMIT_SEPARATOR = "---END---"


def contains_head(branch: str) -> bool:
    """Says whether the branch already contains the commit being released."""
    return (
        subprocess.run(
            ["git", "merge-base", "--is-ancestor", "HEAD", branch],
            capture_output=True,
            check=False,
        ).returncode
        == 0
    )


def get_last_release_branch() -> str | None:
    """Returns the most recent release branch that does not contain the commit being released."""
    try:
        raw_branches = (
            subprocess.check_output(
                ["git", "branch", "-a", "--list", "*release/*.*.x"],
                stderr=subprocess.STDOUT,
            )
            .decode("utf-8")
            .split()
        )
    except subprocess.CalledProcessError:
        return None

    version_pattern = re.compile(r"release/(\d+)\.(\d+)\.x")
    found_versions = []
    for branch in raw_branches:
        # The name git printed, remotes/origin/ prefix included: a release runs on a checkout that
        # has no local branches, so the shortened name is not a reference it can resolve.
        name = branch.strip("* ")
        match = version_pattern.search(name)
        if not match:
            continue

        # A release runs on a tag, so HEAD is detached and the release line being built has no name
        # to compare against. Skipping the lines that already contain HEAD leaves the previous one,
        # which is the one to list the changes against.
        if contains_head(name):
            continue

        major, minor = map(int, match.groups())
        found_versions.append(((major, minor), name))

    if not found_versions:
        return None

    found_versions.sort(key=lambda entry: entry[0], reverse=True)
    return found_versions[0][1]


def get_commit_range() -> str:
    """Returns the revision range of the commits that belong in the changelog."""
    release_branch = get_last_release_branch()
    if not release_branch:
        return "HEAD"

    try:
        merge_base = subprocess.check_output(["git", "merge-base", release_branch, "HEAD"]).decode("utf-8").strip()
    except subprocess.CalledProcessError:
        return "HEAD"

    return f"{merge_base}..HEAD"


def get_commits() -> list[str]:
    """Returns the raw commit messages in the changelog range."""
    log_format = f"%s%n%b%n{COMMIT_SEPARATOR}"
    try:
        raw_log = subprocess.check_output(
            ["git", "log", get_commit_range(), f"--format={log_format}"],
            stderr=subprocess.STDOUT,
        ).decode("utf-8")
    except subprocess.CalledProcessError:
        return []

    return raw_log.split(f"{COMMIT_SEPARATOR}\n")


def build_changelog(commits: list[str]) -> str:
    """Groups the commit messages by type and renders the changelog text."""
    groups: defaultdict[str, list[str]] = defaultdict(list)
    breaking_changes: list[tuple[str, str]] = []

    for raw_message in commits:
        if not raw_message.strip():
            continue

        lines = raw_message.strip().split("\n")
        title = lines[0]
        body = "\n".join(lines[1:])

        match = TITLE_PATTERN.match(title)
        if "BREAKING CHANGE" in body or (match and match.group("breaking")):
            if "BREAKING CHANGE:" in body:
                # The whole footer, to the next blank line: taking one line drops the
                # rest of a description that was wrapped to fit the message.
                footer = body.split("BREAKING CHANGE:")[1].strip()
                description = " ".join(footer.split("\n\n")[0].split())
            else:
                description = "<No specific breaking change description provided.>"
            breaking_changes.append((title, description))

        commit_type = match.group("type") if match else ""
        groups[commit_type if commit_type in TYPE_MAP else UNPARSED_SECTION].append(title)

    sections = ["## 📝 Change Log\n"]
    if breaking_changes:
        sections.append("### 💥 BREAKING CHANGES")
        for title, description in breaking_changes:
            sections.append(f"- **{title}**")
            sections.append(f"  > {description}")
        sections.append("")

    for commit_type, section_title in list(TYPE_MAP.items()) + [(UNPARSED_SECTION, UNPARSED_SECTION)]:
        if commit_type in groups:
            sections.append(f"### {section_title}")
            sections.extend(f"- {message}" for message in groups[commit_type])
            sections.append("")

    return "\n".join(sections)


def main() -> int:
    """Prints the changelog for the current checkout."""
    print(build_changelog(get_commits()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
