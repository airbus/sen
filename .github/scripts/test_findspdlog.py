# === test_findspdlog.py ===============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks which spdlog `Findspdlog.cmake` picks, and that a mismatch stops the build.

spdlog types are in the kernel's public signatures, so a component compiled against a different
spdlog than the kernel links and then misbehaves. The module used to search the system before the
copy Sen installs, and its `REQUIRED_VARS` assertion could not fail because the fallback set the
variable to a path it never checked: a tree with no spdlog at all reported `Found spdlog`.

The module reaches its own copy through `../../include`, so the fixtures reproduce the installed
layout rather than calling the module directly.
"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
MODULE = ROOT / "cmake" / "util" / "Findspdlog.cmake"

# As installed: <prefix>/cmake/sen/Findspdlog.cmake, with the headers at <prefix>/include.
MODULE_SUBDIR = Path("cmake") / "sen"

SEN_SPDLOG = "1.17.0"
OTHER_SPDLOG = "1.9.2"

PROBE = """
cmake_minimum_required(VERSION 3.21)
project(spdlog_probe NONE)
list(APPEND CMAKE_MODULE_PATH "@module_dir@")
find_package(spdlog REQUIRED)
get_target_property(_inc spdlog::spdlog INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(_defs spdlog::spdlog INTERFACE_COMPILE_DEFINITIONS)
message(STATUS "PROBE_INCLUDE=${_inc}")
message(STATUS "PROBE_DEFINITIONS=${_defs}")
"""


def write_headers(include_dir: Path, version: str, with_fmt: str | None = None) -> None:
    """A spdlog header tree as the module recognises one: the header it finds, and the version."""
    major, minor, patch = version.split(".")
    spdlog = include_dir / "spdlog"
    spdlog.mkdir(parents=True, exist_ok=True)
    (spdlog / "spdlog.h").write_text("#pragma once\n", encoding="utf-8")
    (spdlog / "version.h").write_text(
        f"#define SPDLOG_VER_MAJOR {major}\n#define SPDLOG_VER_MINOR {minor}\n#define SPDLOG_VER_PATCH {patch}\n",
        encoding="utf-8",
    )
    if with_fmt:
        (include_dir / "fmt").mkdir(parents=True, exist_ok=True)
        (include_dir / "fmt" / with_fmt).write_text("#pragma once\n", encoding="utf-8")


def write_config_package(prefix: Path, version: str | None) -> None:
    """A config package, which is what conan and a distribution both provide.

    `version` of None is the package that sets no version variable, which several real ones do not.
    """
    config_dir = prefix / "lib" / "cmake" / "spdlog"
    config_dir.mkdir(parents=True, exist_ok=True)
    (config_dir / "spdlog-config.cmake").write_text(
        (f'set(spdlog_VERSION "{version}")\n' if version else "") + "if(NOT TARGET spdlog::spdlog)\n"
        "  add_library(spdlog::spdlog INTERFACE IMPORTED)\n"
        "endif()\n"
        "set(spdlog_FOUND TRUE)\n",
        encoding="utf-8",
    )


def make_prefix(tmp_path: Path, name: str, vendored: str | None, with_fmt: str | None = None) -> Path:
    """An install tree holding the module, and Sen's own spdlog when `vendored` names a version."""
    prefix = tmp_path / name
    module_dir = prefix / MODULE_SUBDIR
    module_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy(MODULE, module_dir / MODULE.name)
    if vendored:
        write_headers(prefix / "include", vendored, with_fmt=with_fmt)
    return prefix


# Whatever spdlog the machine running the tests happens to have must not answer for the fixtures.
# Without this, a host spdlog config package -- or merely CMAKE_PREFIX_PATH in the environment --
# makes most of these tests fail, on a developer's machine and on the runner alike.
ISOLATION = (
    "-DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=OFF",
    "-DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=OFF",
    "-DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=OFF",
    "-DCMAKE_FIND_USE_INSTALL_PREFIX=OFF",
    "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
    "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF",
    # Turning off the environment search also stops cmake finding its own build tool. Nothing is
    # ever built here, only configured, so any existing executable satisfies the generator.
    f"-DCMAKE_MAKE_PROGRAM={sys.executable}",
)

# These reach a find module even with the search paths above turned off.
SEARCH_ENVIRONMENT = ("CMAKE_PREFIX_PATH", "CMAKE_MODULE_PATH", "CMAKE_FRAMEWORK_PATH", "spdlog_DIR", "spdlog_ROOT")


def configure(tmp_path: Path, prefix: Path, extra: list[str] | None = None) -> subprocess.CompletedProcess:
    """Configures the probe against `prefix`, returning the run rather than raising on failure."""
    assert shutil.which("cmake"), "cmake is needed to exercise the find module"
    source = tmp_path / "probe"
    source.mkdir(parents=True, exist_ok=True)
    module_dir = (prefix / MODULE_SUBDIR).as_posix()
    (source / "CMakeLists.txt").write_text(PROBE.replace("@module_dir@", module_dir), encoding="utf-8")
    environment = {name: value for name, value in os.environ.items() if name not in SEARCH_ENVIRONMENT}
    return subprocess.run(
        ["cmake", "-S", str(source), "-B", str(tmp_path / "build"), *ISOLATION, *(extra or [])],
        capture_output=True,
        text=True,
        check=False,
        env=environment,
    )


def chosen_include(run: subprocess.CompletedProcess) -> str:
    """The include directory the module settled on, resolved so the fixtures compare equal."""
    match = re.search(r"PROBE_INCLUDE=(.*)", run.stdout)
    assert match, f"the probe never reported an include directory:\n{run.stdout}\n{run.stderr}"
    return Path(match.group(1).strip()).resolve().as_posix()


def test_sens_own_copy_is_used_when_it_is_the_only_one(tmp_path):
    """The plain case, and the one a tarball install is in."""
    prefix = make_prefix(tmp_path, "sen", vendored=SEN_SPDLOG)
    run = configure(tmp_path, prefix)
    assert run.returncode == 0, run.stderr
    assert chosen_include(run) == (prefix / "include").resolve().as_posix()


def test_sens_own_copy_beats_one_on_the_system(tmp_path):
    """The reason for the fix: a system spdlog is the one least likely to match the kernel."""
    prefix = make_prefix(tmp_path, "sen", vendored=SEN_SPDLOG)
    system = tmp_path / "system"
    write_headers(system / "include", OTHER_SPDLOG)
    run = configure(tmp_path, prefix, [f"-DCMAKE_PREFIX_PATH={system.as_posix()}"])
    assert run.returncode == 0, run.stderr
    assert chosen_include(run) == (prefix / "include").resolve().as_posix()


def test_a_system_copy_is_still_found_without_one_of_ours(tmp_path):
    """A source checkout ships no headers, so the system search has to stay reachable."""
    prefix = make_prefix(tmp_path, "sen", vendored=None)
    system = tmp_path / "system"
    write_headers(system / "include", OTHER_SPDLOG)
    run = configure(tmp_path, prefix, [f"-DCMAKE_PREFIX_PATH={system.as_posix()}"])
    assert run.returncode == 0, run.stderr
    assert chosen_include(run) == (system / "include").resolve().as_posix()


def test_no_spdlog_anywhere_is_reported_as_missing(tmp_path):
    """What the old module could not do: its fallback set the variable to an unchecked path."""
    prefix = make_prefix(tmp_path, "sen", vendored=None)
    run = configure(tmp_path, prefix)
    assert run.returncode != 0, f"a tree with no spdlog configured anyway:\n{run.stdout}"
    assert "Could NOT find spdlog" in run.stdout + run.stderr


@pytest.mark.parametrize("fmt_header", ["core.h", "base.h"], ids=["fmt-core", "fmt-base"])
def test_external_fmt_is_declared_when_fmt_ships_beside_spdlog(tmp_path, fmt_header):
    """Sen builds spdlog against external fmt, and both header trees are installed together.

    core.h is a deprecation stub in current fmt. Were it the only header looked for, its removal
    would stop the define being set and send spdlog to a bundled fmt the install tree has not got.
    """
    prefix = make_prefix(tmp_path, "sen", vendored=SEN_SPDLOG, with_fmt=fmt_header)
    run = configure(tmp_path, prefix)
    assert run.returncode == 0, run.stderr
    assert "PROBE_DEFINITIONS=SPDLOG_FMT_EXTERNAL" in run.stdout


def test_external_fmt_is_not_declared_without_fmt(tmp_path):
    """The other half: spdlog bundles its own fmt, and claiming otherwise breaks the compile."""
    prefix = make_prefix(tmp_path, "sen", vendored=SEN_SPDLOG)
    run = configure(tmp_path, prefix)
    assert run.returncode == 0, run.stderr
    assert "SPDLOG_FMT_EXTERNAL" not in run.stdout


def test_a_config_package_of_another_version_warns(tmp_path):
    """The silent failure this exists for: it links, then the component misbehaves once running.

    A warning rather than a refusal, because a consumer whose package manager chose the version
    often cannot change it, and refusing would stop a build that works today.
    """
    prefix = make_prefix(tmp_path, "sen", vendored=SEN_SPDLOG)
    elsewhere = tmp_path / "elsewhere"
    write_config_package(elsewhere, OTHER_SPDLOG)
    run = configure(tmp_path, prefix, [f"-DCMAKE_PREFIX_PATH={elsewhere.as_posix()}"])
    assert run.returncode == 0, f"a mismatched spdlog stopped the build:\n{run.stdout}\n{run.stderr}"
    output = run.stdout + run.stderr
    assert OTHER_SPDLOG in output and SEN_SPDLOG in output, f"the warning names neither version:\n{output}"
    assert "SEN_SUPPRESS_SPDLOG_VERSION_WARNING" in output, "the warning has to say how to silence it"
    assert (prefix / "include").as_posix() in output, "the warning has to say where Sen's own copy is"


def test_a_config_package_without_a_version_warns(tmp_path):
    """The case a comparison cannot see: nothing to compare, and silence would be the wrong answer."""
    prefix = make_prefix(tmp_path, "sen", vendored=SEN_SPDLOG)
    elsewhere = tmp_path / "elsewhere"
    write_config_package(elsewhere, version=None)
    run = configure(tmp_path, prefix, [f"-DCMAKE_PREFIX_PATH={elsewhere.as_posix()}"])
    assert run.returncode == 0, run.stdout + run.stderr
    assert "does not report a version" in run.stdout + run.stderr


def test_a_config_package_of_the_same_version_is_accepted(tmp_path):
    """The other half: the guard has to stay quiet when the versions agree."""
    prefix = make_prefix(tmp_path, "sen", vendored=SEN_SPDLOG)
    elsewhere = tmp_path / "elsewhere"
    write_config_package(elsewhere, SEN_SPDLOG)
    run = configure(tmp_path, prefix, [f"-DCMAKE_PREFIX_PATH={elsewhere.as_posix()}"])
    assert run.returncode == 0, run.stdout + run.stderr


def test_the_warning_can_be_silenced(tmp_path):
    """Someone who chose their spdlog deliberately should not be nagged on every configure."""
    prefix = make_prefix(tmp_path, "sen", vendored=SEN_SPDLOG)
    elsewhere = tmp_path / "elsewhere"
    write_config_package(elsewhere, OTHER_SPDLOG)
    run = configure(
        tmp_path,
        prefix,
        [f"-DCMAKE_PREFIX_PATH={elsewhere.as_posix()}", "-DSEN_SUPPRESS_SPDLOG_VERSION_WARNING=ON"],
    )
    assert run.returncode == 0, run.stdout + run.stderr
    assert OTHER_SPDLOG not in run.stderr, f"the warning was not silenced:\n{run.stderr}"


def test_the_fixtures_ignore_a_spdlog_on_the_host(tmp_path, monkeypatch):
    """A spdlog reachable only from the environment answered for the fixtures and failed five tests."""
    intruder = tmp_path / "intruder"
    write_config_package(intruder, OTHER_SPDLOG)
    write_headers(intruder / "include", OTHER_SPDLOG)
    monkeypatch.setenv("CMAKE_PREFIX_PATH", str(intruder))
    monkeypatch.setenv("spdlog_DIR", str(intruder / "lib" / "cmake" / "spdlog"))

    prefix = make_prefix(tmp_path, "sen", vendored=None)
    run = configure(tmp_path, prefix)
    assert run.returncode != 0, f"the host's spdlog answered for the fixture:\n{run.stdout}"
    assert "Could NOT find spdlog" in run.stdout + run.stderr
