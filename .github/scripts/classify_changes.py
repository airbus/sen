# === classify_changes.py ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Classifies a change set so the orchestrator can gate the heavy lanes.

Docs-only changes stop after the cheap checks. Every event that carries a base
is classified: a pull request against its base, a push against the commit main
moved from, a merge queue entry against the queue's base. Without one, every
flag fails open and says so.
"""

import argparse
import os
import subprocess
import sys

DOCS_PREFIXES = ("docs/",)
DOCS_SUFFIXES = (".md",)

# docs/ also holds CMakeLists.txt files that every configure parses, so a build
# file is never documentation however it is named or wherever it lives.
BUILD_SUFFIXES = (".cmake", "CMakeLists.txt")

# What the documentation build reads: the handbook, the example tree and the
# component definitions it copies snippets from, the site configuration, the
# conan recipe and profiles it builds with, and its own workflow.
DOCS_BUILD_PREFIXES = ("docs/", "examples/", "components/", ".conan/", ".github/actions/build_documentation/")
DOCS_BUILD_FILES = ("mkdocs.yml", "conanfile.py", "conan.lock", "LICENSE.txt", ".github/workflows/docs_check.yaml")

# What defines the build environment image, and the workflow that validates it.
IMAGE_PREFIXES = ("tools/ci/",)
IMAGE_FILES = (".github/workflows/ci-image.yaml",)


def is_build_path(path: str) -> bool:
    """Returns True when the path takes part in the build."""
    return path.endswith(BUILD_SUFFIXES)


def is_docs_path(path: str) -> bool:
    """Returns True when the path only affects documentation."""
    if is_build_path(path):
        return False

    return path.startswith(DOCS_PREFIXES) or path.endswith(DOCS_SUFFIXES)


def affects_docs_build(path: str) -> bool:
    """Returns True when the path is an input of the documentation build."""
    return path.startswith(DOCS_BUILD_PREFIXES) or path in DOCS_BUILD_FILES or path.endswith(DOCS_SUFFIXES)


def is_docs_build_change(paths: list[str]) -> bool:
    """Returns True when any changed path is an input of the docs build.

    An empty change set counts as docs-affecting, failing open for the same
    reason as is_code_change.
    """
    if not paths:
        return True

    return any(affects_docs_build(path) for path in paths)


def affects_image(path: str) -> bool:
    """Returns True when the path defines the build environment image."""
    return path.startswith(IMAGE_PREFIXES) or path in IMAGE_FILES


def is_image_change(paths: list[str]) -> bool:
    """Returns True when the image has to be rebuilt and checked.

    Documentation is excluded even under tools/ci: the Dockerfile copies
    nothing out of the repository, so a paragraph cannot change the image. The
    exclusion is per path; per change set, one source file promoted a paragraph
    into an image build. An empty change set fails open, as the others do.
    """
    if not paths:
        return True

    return any(affects_image(path) and not is_docs_path(path) for path in paths)


def is_code_change(paths: list[str]) -> bool:
    """Returns True unless every changed path is documentation.

    An empty change set counts as code: failing open runs the full matrix,
    failing closed would skip it on a classification bug.
    """
    if not paths:
        return True

    return any(not is_docs_path(path) for path in paths)


def announce(message: str) -> None:
    """Reports a classification decision in the log and on the run page."""
    print(message)

    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as handle:
            handle.write(f"Change classification: {message}\n")


def commit_exists(sha: str) -> bool:
    """Returns True when the checkout can resolve the sha to a commit."""
    result = subprocess.run(["git", "cat-file", "-e", f"{sha}^{{commit}}"], capture_output=True, check=False)
    return result.returncode == 0


def resolve_base(base_sha: str) -> str:
    """Returns a base that can be diffed, or empty when there is none to use.

    Every rejection is announced: failing open runs everything, which is also
    what a successful classification can produce.
    """
    if not base_sha:
        announce("no base commit for this event, so every lane runs")
        return ""

    # A ref created by the push carries no previous commit.
    if set(base_sha) == {"0"}:
        announce(f"base {base_sha} is all zeros, so every lane runs")
        return ""

    # A rewritten history leaves a base that the checkout cannot reach.
    if not commit_exists(base_sha):
        announce(f"base {base_sha} is not in this checkout, so every lane runs")
        return ""

    return base_sha


def changed_paths(base_sha: str, head: str = "HEAD") -> list[str] | None:
    """Lists the paths that changed since the merge base with base_sha, or None.

    A three-dot diff needs a merge base, so a base resolve_base accepts can
    still share no history with head. That fails open here rather than raising.
    """
    # --no-renames: a rename reports only its destination, so moving a source
    # file into docs/ would otherwise read as a documentation change.
    result = subprocess.run(
        ["git", "diff", "--no-renames", "--name-only", f"{base_sha}...{head}"],
        check=False,
        capture_output=True,
        text=True,
    )

    if result.returncode != 0:
        announce(f"base {base_sha} cannot be diffed against {head}, so every lane runs")
        return None

    return [line for line in result.stdout.splitlines() if line]


def main() -> int:
    """Writes the code=true/false classification to GITHUB_OUTPUT."""
    parser = argparse.ArgumentParser(
        prog="classify_changes",
        description="Classifies the change set for workflow gating.",
    )
    parser.add_argument("--base-sha", default="", help="Commit to diff against; empty means classify nothing.")
    args = parser.parse_args()

    code = True
    docs = True
    image = True
    if base_sha := resolve_base(args.base_sha):
        if (paths := changed_paths(base_sha)) is not None:
            code = is_code_change(paths)
            docs = is_docs_build_change(paths)
            image = is_image_change(paths)

    # A fallback also runs everything, so only a skipped lane is worth naming.
    if skipped := [name for name, run in (("tests", code), ("docs", docs), ("image", image)) if not run]:
        announce(f"skipping {', '.join(skipped)}")

    output_file = os.environ.get("GITHUB_OUTPUT")
    if not output_file:
        raise SystemExit("Error: No output file specified to write to.")

    flags = {"code": code, "docs": docs, "image": image}
    with open(output_file, "a", encoding="utf-8") as handle:
        for name, value in flags.items():
            handle.write(f"{name}={'true' if value else 'false'}\n")

    print(" ".join(f"{name}={'true' if value else 'false'}" for name, value in flags.items()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
