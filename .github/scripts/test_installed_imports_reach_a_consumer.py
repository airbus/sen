# === test_installed_imports_reach_a_consumer.py =======================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that a consumer inherits an installed package's import path, not the producer's build path.

`SEN_IMPORT_DIRS` is the `-i` search path the generator resolves `.stl` imports through. A package
records its own source tree there, and `sen_add_dependency` copies it onto whoever depends on it. For
an installed package that names the tree that built it, which is not where the consumer's interfaces
live.

`searchFile` skips a search path that does not exist, so the cost is not a failed generation: it is
that the path is searched at all. Ahead of the install tree in the order, and present on the
consumer's machine, it resolves an import from the wrong tree without saying so.

`get_external_interfaces` already rewrites `BASE_PATH`, `STL_FILES` and the HLA paths into the
install tree; this is the one of the set that had no `INSTALL_` counterpart.

Properties are decided at configure time, so this needs no build and no generator.
"""

import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
MISC_MODULE = ROOT / "cmake" / "util" / "sen_misc_utils.cmake"

PROJECT = """
cmake_minimum_required(VERSION 3.21)
project(import_dirs NONE)

include(@misc_module@)

add_library(producer INTERFACE)
add_library(consumer INTERFACE)

# As a package records it: the tree that built the package.
set_property(TARGET producer PROPERTY SEN_IMPORT_DIRS -i /build/of/the/producer)
@installed@

sen_add_dependency(consumer INTERFACE producer)

get_target_property(_inherited consumer SEN_IMPORT_DIRS)
message(STATUS "INHERITED|${_inherited}")
"""

# What get_external_interfaces records once a package has been installed somewhere.
INSTALLED = "set_property(TARGET producer PROPERTY INSTALL_SEN_IMPORT_DIRS -i /the/install/tree/interfaces/)"

needs_cmake = pytest.mark.skipif(shutil.which("cmake") is None, reason="needs cmake")


def inherited(tmp_path: Path, *, installed: bool) -> str:
    """Configures a consumer depending on a producer, returning the import path it inherited."""
    source, build = tmp_path / "proj", tmp_path / "build"
    source.mkdir(parents=True)
    body = PROJECT.replace("@misc_module@", MISC_MODULE.as_posix()).replace(
        "@installed@", INSTALLED if installed else ""
    )
    (source / "CMakeLists.txt").write_text(body, encoding="utf-8")

    finished = subprocess.run(
        ["cmake", "-S", str(source), "-B", str(build)],
        capture_output=True,
        text=True,
        # Asserted below, so a failure can show what CMake said.
        check=False,
    )
    assert finished.returncode == 0, finished.stdout + finished.stderr
    marks = [line.split("INHERITED|", 1)[1] for line in finished.stdout.splitlines() if "INHERITED|" in line]
    assert len(marks) == 1, finished.stdout
    return marks[0]


@needs_cmake
def test_a_consumer_inherits_an_import_path_at_all(tmp_path):
    """Passes either way: it proves the probe exercises sen_add_dependency."""
    assert "-i" in inherited(tmp_path, installed=False)


@needs_cmake
def test_a_build_tree_package_still_passes_its_own_path(tmp_path):
    """The in-tree case, where the producer's path is the right one and must not change."""
    assert inherited(tmp_path, installed=False) == "-i;/build/of/the/producer"


@needs_cmake
def test_an_installed_package_passes_its_installed_path(tmp_path):
    """The case that reaches a consumer's generator as a directory that is not there."""
    assert inherited(tmp_path, installed=True) == "-i;/the/install/tree/interfaces/", (
        "the consumer inherited the producer's build tree, which does not exist on its machine"
    )
