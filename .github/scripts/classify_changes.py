# === classify_changes.py ==============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Classifies a change set so the orchestrator can gate the heavy lanes.

Docs-only pull requests stop after the cheap checks. Without a base commit
to diff against (pushes, manual dispatch), everything counts as code, and the
image is rebuilt and checked.
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
    nothing out of the repository, so a paragraph cannot change the image. An
    empty change set fails open, as the other classifications do.
    """
    if not paths:
        return True

    if not is_code_change(paths):
        return False

    return any(affects_image(path) for path in paths)


def is_code_change(paths: list[str]) -> bool:
    """Returns True unless every changed path is documentation.

    An empty change set counts as code: failing open runs the full matrix,
    failing closed would skip it on a classification bug.
    """
    if not paths:
        return True

    return any(not is_docs_path(path) for path in paths)


def changed_paths(base_sha: str) -> list[str]:
    """Lists the paths that changed since the merge base with base_sha."""
    # --no-renames: a rename reports only its destination, so moving a source
    # file into docs/ would otherwise read as a documentation change.
    result = subprocess.run(
        ["git", "diff", "--no-renames", "--name-only", f"{base_sha}...HEAD"],
        check=True,
        capture_output=True,
        text=True,
    )

    return [line for line in result.stdout.splitlines() if line]


def main() -> int:
    """Writes the code=true/false classification to GITHUB_OUTPUT."""
    parser = argparse.ArgumentParser(
        prog="classify_changes",
        description="Classifies the change set for workflow gating.",
    )
    parser.add_argument(
        "--base-sha", default="", help="Base commit of the pull request; empty means not a pull request."
    )
    args = parser.parse_args()

    code = True
    docs = True
    image = True
    if args.base_sha:
        paths = changed_paths(args.base_sha)
        code = is_code_change(paths)
        docs = is_docs_build_change(paths)
        image = is_image_change(paths)

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
