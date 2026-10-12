# === test_the_cmake_config_installs_where_cmake_looks.py ==============================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that `find_package(sen)` reaches the installed config from the install prefix alone.

CMake searches a prefix for `<prefix>/lib/cmake/<name>`, the prefix root, and a short list of
siblings. It does not search `<prefix>/cmake/<name>`, so a consumer given the prefix gets "could not
find sen" and has to learn a suffix nobody else needs.

The destination is read out of `install.cmake` rather than restated here, so this fails when the
default moves somewhere CMake still cannot reach rather than when it stops matching a hardcoded path.
Stub configs and no build: what is under test is where the file goes, not what it contains.
"""

import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
INSTALL_MODULE = ROOT / "cmake" / "util" / "install.cmake"
# CMAKE_INSTALL_LIBDIR is pinned here rather than left to GNUInstallDirs, so the destination below
# expands to one known directory. Read rather than restated, for the same reason as the destination.
INTERNAL_MODULE = ROOT / "cmake" / "util" / "sen_internal_utils.cmake"

CONSUMER = """
cmake_minimum_required(VERSION 3.21)
project(consumer NONE)
find_package(sen REQUIRED)
message(STATUS "RESOLVED|${sen_DIR}")
"""

needs_cmake = pytest.mark.skipif(shutil.which("cmake") is None, reason="needs cmake")


def installed_cmakedir() -> str:
    """The default `CMAKE_INSTALL_CMAKEDIR`, read from the module that sets it, with libdir resolved."""
    text = INSTALL_MODULE.read_text(encoding="utf-8")
    match = re.search(r'set\(CMAKE_INSTALL_CMAKEDIR\s+"([^"]+)"', text)
    assert match, "CMAKE_INSTALL_CMAKEDIR is not set the way this test reads it"
    where = match.group(1)

    libdir = re.search(r"set\(CMAKE_INSTALL_LIBDIR\s+([^\s)]+)\)", INTERNAL_MODULE.read_text(encoding="utf-8"))
    assert libdir, "CMAKE_INSTALL_LIBDIR is not pinned the way this test reads it"
    where = where.replace("${CMAKE_INSTALL_LIBDIR}", libdir.group(1))

    assert "${" not in where, f"unresolved variable in the destination: {where}"
    return where


def resolves_from(tmp_path: Path, config_dir: str, *, prefix_path: str | None = None) -> str | None:
    """Puts a stub sen config at `config_dir` under a prefix, then configures a consumer against it.

    Returns the directory `find_package` resolved to, or None when it found nothing.
    """
    prefix = tmp_path / "prefix"
    (prefix / config_dir).mkdir(parents=True, exist_ok=True)
    (prefix / config_dir / "sen-config.cmake").write_text("set(sen_FOUND TRUE)\n", encoding="utf-8")

    source, build = tmp_path / "consumer", tmp_path / "build"
    source.mkdir(parents=True, exist_ok=True)
    (source / "CMakeLists.txt").write_text(CONSUMER, encoding="utf-8")

    finished = subprocess.run(
        ["cmake", "-S", str(source), "-B", str(build), f"-DCMAKE_PREFIX_PATH={prefix_path or prefix}"],
        capture_output=True,
        text=True,
        # Not finding the package is the result under test, not an error in the harness.
        check=False,
    )
    if finished.returncode != 0:
        return None
    marks = [line.split("RESOLVED|", 1)[1] for line in finished.stdout.splitlines() if "RESOLVED|" in line]
    assert len(marks) == 1, finished.stdout
    return marks[0]


@needs_cmake
def test_cmake_reaches_a_config_in_its_own_search_path(tmp_path):
    """Passes either way: it proves the harness can find a config CMake is meant to find."""
    assert resolves_from(tmp_path, "lib/cmake/sen") is not None


@needs_cmake
def test_a_config_outside_the_search_path_is_not_found(tmp_path):
    """Passes either way. The negative has to be reachable or the check below cannot fail."""
    assert resolves_from(tmp_path, "share/elsewhere/sen") is None


@needs_cmake
def test_the_installed_config_is_found_from_the_prefix(tmp_path):
    """The case a consumer hits: the prefix alone, with the config where Sen installs it."""
    where = installed_cmakedir()
    assert resolves_from(tmp_path, where) is not None, (
        f"find_package(sen) did not reach a config at <prefix>/{where}, so a consumer given the "
        "prefix has to append a suffix to find Sen"
    )
