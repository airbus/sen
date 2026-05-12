import subprocess
import re
from collections import defaultdict

# Mapping types to icons/headers
TYPE_MAP = {
    "feat": "✨ Features",
    "fix": "🐛 Bug Fixes",
    "ci": "👷 Continuous Integration",
    "docs": "📚 Documentation",
    "style": "💎 Style",
    "refactor": "🔨 Refactor",
    "perf": "🚀 Performance",
    "test": "🚨 Tests",
    "chore": "📦 Chores",
}


def get_current_branch() -> str:
    """Returns the current branch we are on."""
    return (
        subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"])
        .decode("utf-8")
        .strip()
    )


def get_last_release_branch():
    """Returns the last release branch (the last one before the this one)."""
    try:
        current_branch = get_current_branch()
        raw_branches = (
            subprocess.check_output(
                ["git", "branch", "-a", "--list", "*release/*.*.x"],
                stderr=subprocess.STDOUT,
            )
            .decode("utf-8")
            .split()
        )

        version_pattern = re.compile(r"release/(\d+)\.(\d+)\.x")
        found_versions = []

        for b in raw_branches:
            clean_name = b.strip("* ").replace("remotes/origin/", "")
            if clean_name == current_branch:
                continue

            match = version_pattern.search(clean_name)
            if match:
                major, minor = map(int, match.groups())
                found_versions.append(((major, minor), clean_name))

        if not found_versions:
            return None

        found_versions.sort(key=lambda x: x[0], reverse=True)
        return found_versions[0][1]
    except subprocess.CalledProcessError:
        return None


def get_commit_range() -> str:
    """Compute the range of commits we need to include."""
    release_branch = get_last_release_branch()
    if not release_branch:
        return "HEAD"
    try:
        merge_base = (
            subprocess.check_output(["git", "merge-base", release_branch, "HEAD"])
            .decode("utf-8")
            .strip()
        )
        return f"{merge_base}..HEAD"
    except subprocess.CalledProcessError:
        return "HEAD"


def get_commits() -> list[str]:
    """Returns a list of formatted commit messages."""
    revision_range = get_commit_range()
    log_format = "%s%n%b%n---END---"
    try:
        raw_log = subprocess.check_output(
            ["git", "log", revision_range, f"--format={log_format}"],
            stderr=subprocess.STDOUT,
        ).decode("utf-8")
        return raw_log.split("---END---\n")
    except subprocess.CalledProcessError:
        return []


def parse_and_print() -> None:
    """Parses the commit log and then prints the message titles in a grouped manner."""
    commits = get_commits()
    groups = defaultdict(list)
    breaking_changes = []  # List of tuples: (title, description)

    pattern = re.compile(
        r"^(?P<type>\w+)(?:\[(?P<scope>[^)]+)\])?(?P<breaking>!)?: (?P<subject>.+)"
    )

    for raw_msg in commits:
        if not raw_msg.strip():
            continue

        lines = raw_msg.strip().split("\n")
        title = lines[0]
        body = "\n".join(lines[1:])

        match = pattern.match(title)
        is_breaking = "BREAKING CHANGE" in body or (match and match.group("breaking"))

        if is_breaking:
            # Extract description from body or fallback to the title
            if "BREAKING CHANGE:" in body:
                desc = body.split("BREAKING CHANGE:")[1].strip().split("\n")[0]
            else:
                desc = "<No specific breaking change description provided.>"
            breaking_changes.append((title, desc))

        if match:
            ctype = match.group("type")
            groups[ctype].append(title)

    print("## 📝 Change Log\n")

    # 1. Breaking Changes with indentation
    if breaking_changes:
        print("### 💥 BREAKING CHANGES")
        for title, desc in breaking_changes:
            print(f"- **{title}**")
            print(f"  > {desc}")
        print()

    # 2. Standard Groups
    for ctype, section_title in TYPE_MAP.items():
        if ctype in groups:
            print(f"### {section_title}")
            for msg in groups[ctype]:
                print(f"- {msg}")
            print()


if __name__ == "__main__":
    parse_and_print()
