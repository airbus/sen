# === test_classify_changes.py =========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the docs-only classification that gates the heavy workflow lanes."""

from classify_changes import is_code_change, is_docs_build_change, is_image_change


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
