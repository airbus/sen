# === test_classify_changes.py =========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the docs-only classification that gates the heavy workflow lanes."""

import os
import subprocess
import sys

import classify_changes
import pytest
from classify_changes import changed_paths, is_code_change, is_docs_build_change, is_image_change, resolve_base


@pytest.fixture(autouse=True)
def keep_out_of_the_live_summary(monkeypatch):
    """Stops announce() writing into the job summary of the run testing it.

    python-checks runs inside Actions, where GITHUB_STEP_SUMMARY is always set,
    so without this every run published its own test messages as real ones.
    """
    monkeypatch.delenv("GITHUB_STEP_SUMMARY", raising=False)


def test_docs_directory_only_is_not_code():
    """Changes confined to docs/ stop after the cheap checks."""
    assert not is_code_change(["docs/users_guide/util_library.md", "docs/index.md"])


def test_markdown_anywhere_is_not_code():
    """Markdown outside docs/ also counts as documentation."""
    assert not is_code_change(["README.md", "examples/config/0_counter/readme.md"])


def test_mixed_changes_are_code():
    """One source file among documentation runs the full matrix."""
    assert is_code_change(["docs/index.md", "libs/core/src/base/hash32.cpp"])


def test_workflow_changes_are_code():
    """CI definition changes run the full matrix."""
    assert is_code_change([".github/workflows/main.yaml"])


def test_empty_change_set_is_code():
    """No diff information fails open into the full matrix."""
    assert is_code_change([])


def test_docs_lookalike_source_is_code():
    """Paths that merely mention docs are not documentation."""
    assert is_code_change(["libs/core/docs_generator.cpp", "apps/cli_gen/src/mkdocs/mkdocs_generator.cpp"])


def test_docs_paths_trigger_the_docs_build():
    """Handbook, snippet-source and site-configuration paths build the docs."""
    assert is_docs_build_change(["docs/index.md"])
    assert is_docs_build_change(["examples/config/14_jsonrpc/3_explorer.yaml"])
    assert is_docs_build_change(["mkdocs.yml"])
    assert is_docs_build_change(["README.md"])


def test_pure_source_changes_skip_the_docs_build():
    """Source-only changes do not build the docs on the pull request."""
    assert not is_docs_build_change(["libs/core/src/base/hash32.cpp", ".github/workflows/main.yaml"])


def test_empty_change_set_builds_the_docs():
    """No diff information fails open into building the docs."""
    assert is_docs_build_change([])


def test_build_files_under_docs_are_code():
    """docs/CMakeLists.txt is parsed by every configure, so it is not documentation."""
    assert is_code_change(["docs/CMakeLists.txt"])
    assert is_code_change(["docs/doxygen/CMakeLists.txt"])
    assert is_code_change(["cmake/util/test.cmake"])


def test_non_markdown_documentation_is_still_documentation():
    """The docs/ prefix rule carries its own weight, independent of the .md rule."""
    assert not is_code_change(["docs/requirements.txt"])
    assert not is_code_change(["docs/assets/logo.svg"])


def test_docs_build_inputs_beyond_the_handbook():
    """The documentation build reads the recipe, the profiles and its own action."""
    assert is_docs_build_change(["conanfile.py"])
    assert is_docs_build_change(["conan.lock"])
    assert is_docs_build_change([".conan/profiles/sen_build_docs"])
    assert is_docs_build_change(["LICENSE.txt"])
    assert is_docs_build_change(["components/ether/stl/configuration.stl"])
    assert is_docs_build_change([".github/actions/build_documentation/action.yaml"])
    assert is_docs_build_change([".github/workflows/docs_check.yaml"])


def test_a_source_change_still_skips_the_docs_build():
    """Source-only changes do not build the documentation on the pull request."""
    assert not is_docs_build_change(["libs/core/src/base/hash32.cpp"])
    assert not is_docs_build_change([".github/workflows/main.yaml"])


def test_the_image_is_checked_when_its_definition_changes():
    """A change to the environment definition rebuilds and checks the image."""
    assert is_image_change(["tools/ci/Dockerfile"])
    assert is_image_change(["tools/ci/runtime.Dockerfile"])
    assert is_image_change([".github/workflows/ci-image.yaml"])
    assert is_image_change(["libs/core/src/base/hash32.cpp", "tools/ci/Dockerfile"])


def test_the_image_is_left_alone_by_unrelated_changes():
    """Nothing outside the environment definition triggers an image build."""
    assert not is_image_change(["libs/core/src/base/hash32.cpp"])
    assert not is_image_change([".github/workflows/nightly.yaml"])
    assert not is_image_change(["tools/release/publish.py"])


def test_documentation_never_triggers_an_image_build():
    """The Dockerfile copies nothing from the repository, so prose cannot change it."""
    assert not is_image_change(["tools/ci/architecture.md"])
    assert not is_image_change(["tools/ci/architecture.md", "docs/index.md"])
    # a build file alongside the prose makes it a code change again
    assert is_image_change(["tools/ci/architecture.md", "tools/ci/Dockerfile"])


def test_an_empty_change_set_checks_the_image():
    """Without diff information the image is checked, like every other lane."""
    assert is_image_change([])


def test_prose_under_tools_ci_does_not_ride_in_on_an_unrelated_change():
    """The exclusion is per path, not per change set.

    This repository updates architecture.md in the same commit as the pipeline
    change it describes, so per change set it fired almost every time.
    """
    assert not is_image_change(["tools/ci/architecture.md", ".github/workflows/nightly.yaml"])
    assert not is_image_change(["tools/ci/architecture.md", "libs/core/src/base/hash32.cpp"])
    # A real input alongside the prose still counts.
    assert is_image_change(["tools/ci/architecture.md", "tools/ci/Dockerfile"])
    assert is_image_change(["tools/ci/architecture.md", "tools/ci/CMakeLists.txt"])


def test_no_base_classifies_nothing(capsys):
    """An event without a base runs everything, and says so."""
    assert not resolve_base("")
    assert "every lane runs" in capsys.readouterr().out


def test_an_all_zero_base_classifies_nothing(capsys):
    """A ref created by the push carries no previous commit."""
    assert not resolve_base("0" * 40)
    assert "all zeros" in capsys.readouterr().out


def test_a_base_the_checkout_cannot_reach_classifies_nothing(monkeypatch, capsys):
    """A rewritten history leaves a base that git cannot resolve."""
    monkeypatch.setattr(classify_changes, "commit_exists", lambda _sha: False)
    assert not resolve_base("a" * 40)
    assert "not in this checkout" in capsys.readouterr().out


def test_a_reachable_base_is_used(monkeypatch):
    """The ordinary case: the base is returned and the diff is taken."""
    monkeypatch.setattr(classify_changes, "commit_exists", lambda _sha: True)
    assert resolve_base("a" * 40) == "a" * 40


def test_a_rejection_reaches_the_job_summary(tmp_path, monkeypatch):
    """The summary write is the half that matters, so it is asserted here."""
    summary = tmp_path / "summary.md"
    monkeypatch.setenv("GITHUB_STEP_SUMMARY", str(summary))

    assert not resolve_base("")

    assert "every lane runs" in summary.read_text(encoding="utf-8")


def test_a_base_with_no_common_history_classifies_nothing(tmp_path, monkeypatch, capsys):
    """A base can resolve and still share no history, which the three-dot diff needs.

    resolve_base cannot see this: the commit is there and reachable.
    """

    def git(*args: str) -> None:
        """Runs git with the identity per invocation and its config inside tmp_path.

        Contained the same way as test_create_changelog.py, where the reason is.
        """
        env = {
            **os.environ,
            "GIT_CONFIG_GLOBAL": str(tmp_path / "gitconfig-global"),
            "GIT_CONFIG_SYSTEM": str(tmp_path / "gitconfig-system"),
        }
        identity = ["-c", "user.name=Test", "-c", "user.email=test@example.com"]
        subprocess.run(["git", "-C", str(tmp_path), *identity, *args], check=True, capture_output=True, env=env)

    git("init", "-q", "-b", "one")
    (tmp_path / "file.txt").write_text("one")
    git("add", "file.txt")
    git("-c", "commit.gpgsign=false", "commit", "-qm", "one")

    # An orphan branch has no parent, so the two lines share no commit at all.
    git("checkout", "-q", "--orphan", "two")
    (tmp_path / "other.txt").write_text("two")
    git("add", "other.txt")
    git("-c", "commit.gpgsign=false", "commit", "-qm", "two")
    monkeypatch.chdir(tmp_path)

    assert changed_paths("one", head="two") is None
    assert "cannot be diffed" in capsys.readouterr().out


def test_the_skipped_lanes_reach_the_job_summary(tmp_path, monkeypatch):
    """A lane that did not run is what the run page cannot otherwise show."""
    summary = tmp_path / "summary.md"
    monkeypatch.setenv("GITHUB_STEP_SUMMARY", str(summary))
    monkeypatch.setenv("GITHUB_OUTPUT", str(tmp_path / "output.txt"))
    monkeypatch.setattr(classify_changes, "resolve_base", lambda _sha: "a base")
    monkeypatch.setattr(classify_changes, "changed_paths", lambda _base: ["docs/index.md"])
    monkeypatch.setattr(sys, "argv", ["classify_changes", "--base-sha", "a base"])

    assert classify_changes.main() == 0

    assert "skipping tests, image" in summary.read_text(encoding="utf-8")
