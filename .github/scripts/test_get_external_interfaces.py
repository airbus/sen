#!/usr/bin/env python3
# === test_get_external_interfaces.py ==================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that get_external_interfaces() says something when a package exports no interface paths.

Interface paths are exported only when a package passes EXPORT_INTERFACES. A package that installs
its .stl files and forgets the keyword reaches this function with nothing to rewrite, and the
consumer finds out later as a type missing from generated code, in a place that does not name the
package. The path-rewriting behaviour itself is covered by the conan test package, which can hold
an install tree; these tests only need a configure.
"""

import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "cmake" / "util" / "sen_misc_utils.cmake"

WARNING = "exports no interface paths"

# An imported target carries properties without needing sources, a compiler or a generator step.
PROBE = """
cmake_minimum_required(VERSION 3.20)
project(probe NONE)
include("@module@")

add_library(probe_target INTERFACE IMPORTED)
@properties@

get_external_interfaces(TARGET probe_target INSTALLATION_DIR "@install_dir@")
"""

PROPERTY_LINES = {
    "stl": 'set_target_properties(probe_target PROPERTIES BASE_PATH "@base@" STL_FILES "@base@/stl/a.stl")',
    "hla_dirs": 'set_target_properties(probe_target PROPERTIES BASE_PATH "@base@" HLA_FOM_DIRS "@base@/hla")',
    "hla_mappings": (
        'set_target_properties(probe_target PROPERTIES BASE_PATH "@base@" HLA_MAPPINGS "@base@/hla/m.xml")'
    ),
}


def configure(tmp_path: Path, properties: str) -> subprocess.CompletedProcess:
    """Configures a probe that calls the function, returning the run rather than raising."""
    assert shutil.which("cmake"), "cmake is needed to exercise the module"
    base = tmp_path / "build_tree"
    install_dir = tmp_path / "installed"
    for relative in ("stl/a.stl", "hla/m.xml"):
        path = base / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("", encoding="utf-8")
        installed = install_dir / "interfaces" / relative
        installed.parent.mkdir(parents=True, exist_ok=True)
        installed.write_text("", encoding="utf-8")
    (install_dir / "hla").mkdir(parents=True, exist_ok=True)
    (install_dir / "hla" / "m.xml").write_text("", encoding="utf-8")

    source = tmp_path / "probe"
    source.mkdir(parents=True, exist_ok=True)
    text = (
        PROBE.replace("@module@", MODULE.as_posix())
        .replace("@properties@", properties.replace("@base@", base.as_posix()))
        .replace("@install_dir@", install_dir.as_posix())
    )
    (source / "CMakeLists.txt").write_text(text, encoding="utf-8")
    return subprocess.run(
        [
            "cmake",
            "-S",
            str(source),
            "-B",
            str(tmp_path / "build"),
            f"-DCMAKE_MAKE_PROGRAM={sys.executable}",
        ],
        capture_output=True,
        text=True,
        check=False,
    )


def test_a_target_with_no_interface_paths_is_warned_about(tmp_path):
    """The case a forgotten EXPORT_INTERFACES produces."""
    run = configure(tmp_path, properties="")
    assert run.returncode == 0, run.stderr
    assert WARNING in run.stderr, run.stderr


def test_the_warning_names_the_keyword_to_pass(tmp_path):
    """A warning that does not say what to do sends the reader to the wrong file."""
    run = configure(tmp_path, properties="")
    assert "EXPORT_INTERFACES" in run.stderr, run.stderr


def test_the_warning_names_the_target(tmp_path):
    """A consumer configures several packages, so the message has to say which one."""
    run = configure(tmp_path, properties="")
    assert "probe_target" in run.stderr, run.stderr


@pytest.mark.parametrize("kind", sorted(PROPERTY_LINES))
def test_any_exported_interface_path_silences_it(tmp_path, kind):
    """Each of the three properties on its own is a package that did pass the keyword."""
    run = configure(tmp_path, properties=PROPERTY_LINES[kind])
    assert run.returncode == 0, run.stderr
    assert WARNING not in run.stderr, run.stderr


def test_the_function_still_rewrites_a_path_it_was_given(tmp_path):
    """The warning must not have replaced the work: a run with properties reaches the install tree."""
    run = configure(tmp_path, properties=PROPERTY_LINES["stl"])
    assert run.returncode == 0, run.stderr
    assert "could not be found on system" not in run.stderr, run.stderr
