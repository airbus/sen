#!/usr/bin/env python3
# === test_the_conan_lane_runs_the_test_package.py =====================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""Checks that the packaging lane names the test package it exercises.

Conan looks for `test_package/` beside the recipe and there is none, so `conan create` without
`--test-folder` packages Sen, runs no consumer and exits 0.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github" / "workflows" / "conan.yaml"
STEP_NAME = "Create conan package"
TEST_FOLDER = Path(".conan") / "test_packages" / "package"


def step() -> str:
    """The step's own block: from its name to the next step at the same indentation."""
    text = WORKFLOW.read_text(encoding="utf-8")
    start = text.find(f"- name: {STEP_NAME}")
    assert start != -1, f"no step named {STEP_NAME!r} in conan.yaml"
    rest = text[start:]
    following = re.search(r"\n      - name:", rest)
    return rest[: following.start()] if following else rest


def test_the_step_names_the_test_folder():
    """`conan create` is given the test package explicitly, because the default path does not exist."""
    assert "--test-folder" in step(), "conan create would run no test package"
    assert TEST_FOLDER.as_posix() in step(), f"the step does not name {TEST_FOLDER.as_posix()}"


def test_the_default_test_package_path_is_still_absent():
    """The reason the flag is required. A `test_package/` at the root would make it optional."""
    assert not (ROOT / "test_package").exists(), (
        "a test_package/ directory now exists at the root, so conan would find it without "
        "--test-folder: either drop the flag or drop this test"
    )


def test_the_test_folder_the_step_names_exists():
    """A renamed fixture directory would leave the flag pointing at nothing."""
    named = ROOT / TEST_FOLDER
    assert named.is_dir(), f"{TEST_FOLDER.as_posix()} is not a directory"
    assert (named / "conanfile.py").is_file(), f"{TEST_FOLDER.as_posix()} holds no conanfile.py"
