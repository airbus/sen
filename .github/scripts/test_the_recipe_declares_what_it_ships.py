# === test_the_recipe_declares_what_it_ships.py ========================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks the package type and topics the recipe declares.

Conan decides from `package_type` what a dependency propagates to a consumer. Sen ships shared
libraries. `topics` is what a registry classifies a package by, and Sen is general purpose.

Read through `conan inspect`, so a change to how the recipe computes either is still covered.
"""

import json
import os
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]

needs_conan = pytest.mark.skipif(shutil.which("conan") is None, reason="needs conan")

LIBRARY_TYPES = ("shared-library", "static-library", "library", "header-library")


def inspected(tmp_path: Path) -> dict:
    """The recipe's evaluated attributes, from a conan cache of this test's own."""
    environment = dict(os.environ, CONAN_HOME=str(tmp_path / "conan-home"))
    finished = subprocess.run(
        ["conan", "inspect", ".", "--format=json"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        env=environment,
        # Asserted below, so a failure can show what conan said.
        check=False,
    )
    assert finished.returncode == 0, finished.stdout + finished.stderr
    return json.loads(finished.stdout)


@needs_conan
def test_the_probe_reads_the_real_recipe(tmp_path):
    """The licence, which is set and known, so a probe that reads nothing is visible."""
    assert inspected(tmp_path)["license"] == "Apache-2.0"


@needs_conan
def test_the_recipe_says_what_kind_of_package_it_is(tmp_path):
    """The recipe declares a library package type."""
    declared = inspected(tmp_path)["package_type"]
    assert declared in LIBRARY_TYPES, f"package_type is {declared!r}; Sen ships shared libraries and should say so"


@needs_conan
def test_the_recipe_does_not_file_sen_as_simulation_software(tmp_path):
    """The topics do not list Sen as simulation software."""
    topics = inspected(tmp_path)["topics"] or ()
    assert "simulation" not in topics, (
        f"topics is {list(topics)}; Sen is general purpose and the policy is to say virtual time"
    )
