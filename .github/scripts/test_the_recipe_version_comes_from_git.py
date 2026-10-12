# === test_the_recipe_version_comes_from_git.py ========================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that the recipe derives its version from git wherever git has a checkout.

`set_version` falls back to a fictive `0.0.0` when it finds no repository, and the package then
carries a version that answers nothing. In a **worktree** -- and in a submodule -- `.git` is a file
rather than a directory, which a directory test rejects. `git_info.cmake:99` says so in a comment and
handles it; the recipe is the one place that does not.

`conan inspect` evaluates `set_version` without exporting anything, so this costs a fifth of a second
and needs no build. The repository it runs against holds only the recipe, which is enough for an
inspect and keeps the fixture small.
"""

import json
import os
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
RECIPE = ROOT / "conanfile.py"
TAG = "9.9.9"

needs_conan = pytest.mark.skipif(shutil.which("conan") is None, reason="needs conan")


def git(where: Path, *args: str) -> None:
    """Runs git in `where`, with an identity so a commit does not depend on the host's config."""
    subprocess.run(
        ["git", "-c", "user.email=t@example.invalid", "-c", "user.name=t", *args],
        cwd=where,
        check=True,
        capture_output=True,
        text=True,
    )


def tiny_repo(tmp_path: Path) -> Path:
    """A repository holding the real recipe, one commit and one tag."""
    main = tmp_path / "main"
    main.mkdir(parents=True)
    shutil.copy(RECIPE, main / "conanfile.py")
    git(main, "init", "-q", "-b", "main")
    git(main, "add", "conanfile.py")
    git(main, "commit", "-q", "-m", "the recipe")
    git(main, "tag", TAG)
    return main


def inspected_version(tmp_path: Path, folder: Path) -> str:
    """The version `conan inspect` reports for the recipe in `folder`."""
    environment = dict(os.environ, CONAN_HOME=str(tmp_path / "conan-home"))
    finished = subprocess.run(
        ["conan", "inspect", ".", "--format=json"],
        cwd=folder,
        capture_output=True,
        text=True,
        env=environment,
        # Asserted below so a failure can show what conan said.
        check=False,
    )
    assert finished.returncode == 0, finished.stdout + finished.stderr
    return json.loads(finished.stdout)["version"]


@needs_conan
def test_a_plain_checkout_takes_its_version_from_git(tmp_path):
    """Passes either way: it proves the probe reaches `set_version` and git answers."""
    assert inspected_version(tmp_path, tiny_repo(tmp_path)) == TAG


@needs_conan
def test_a_tree_with_no_repository_falls_back(tmp_path):
    """Also passes either way. The fallback has to be reachable or the check below cannot fail."""
    bare = tmp_path / "exported"
    bare.mkdir(parents=True)
    shutil.copy(RECIPE, bare / "conanfile.py")
    assert inspected_version(tmp_path, bare) == "0.0.0"


@needs_conan
def test_a_worktree_takes_its_version_from_git(tmp_path):
    """A worktree's `.git` is a file. The version must still come from git, not the fallback."""
    main = tiny_repo(tmp_path)
    tree = tmp_path / "tree"
    git(main, "worktree", "add", "-q", "--detach", str(tree), TAG)
    assert (tree / ".git").is_file(), "the fixture is not exercising the worktree case"

    assert inspected_version(tmp_path, tree) == TAG, (
        "the recipe fell back to a fictive version in a worktree, so a package built there "
        "carries a version that answers nothing"
    )
