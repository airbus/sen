# === test_unit_suite_properties.py ====================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that a unit suite's test properties survive the expansion into gtest_discover_tests.

`gtest_discover_tests(... PROPERTIES ...)` reads its arguments as key/value pairs, so a value that
expands to anything but one item shifts everything after it. A sanitizer build sets two or more
`*SAN_OPTIONS`, which left `ENVIRONMENT` holding one of them and consumed the suite's
`EXTRA_PROPERTIES` as a property name. `LABELS` has the same shape once a suite is `FLAKY`.

Reads the arguments Sen's real function passes, with `gtest_discover_tests` stubbed: no gtest and no
build, since the properties are decided at configure time. The stub reads `ARGV<n>` because a
`foreach(... IN LISTS ...)` re-splits on semicolons and would flatten the escaping under test.
"""

import os
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TEST_MODULE = ROOT / "cmake" / "util" / "test.cmake"

# Sen's function reaches for these; stubbing them keeps the function itself real.
STUBBED_COMMANDS = ("sen_configure_target", "sen_skip_linting", "sen_enable_static_analysis")
STUBBED_TARGETS = ("GTest_gmock_main", "sen_coverage_flags", "sen_sanitizer_options")

PROJECT = """
cmake_minimum_required(VERSION 3.21)
project(suite_props CXX)

@stubs@
add_library(GTest::gmock_main ALIAS GTest_gmock_main)
add_custom_target(run_unit_tests)

@suppressions@

include(@test_module@)

# After the include: test.cmake pulls in GoogleTest, which defines the real
# gtest_discover_tests. CMake resolves a command at call time, so this wins.
function(gtest_discover_tests target)
  set(_seen FALSE)
  math(EXPR _last "${ARGC} - 1")
  foreach(_i RANGE 0 ${_last})
    set(_a "${ARGV${_i}}")
    if(_seen)
      message(STATUS "ARG|${_a}")
    endif()
    if(_a STREQUAL "PROPERTIES")
      set(_seen TRUE)
    endif()
  endforeach()
endfunction()

add_sen_unit_test_suite(probe_test probe.cpp @flaky@ EXTRA_PROPERTIES TIMEOUT 60)
"""

SANITIZED = """
set(LSAN_SUPPRESSION_FILE "/tmp/lsan_ignorelist.txt")
set(ASAN_SUPPRESSION_FILE "/tmp/asan_ignorelist.txt")
set(SEN_SANITIZER_LOG_OPTION ":log_path=/tmp/report")
"""

needs_cmake = pytest.mark.skipif(shutil.which("cmake") is None, reason="needs cmake")


def properties(tmp_path: Path, *, sanitized: bool, flaky: bool) -> list[str]:
    """Configures a probe and returns the PROPERTIES arguments, one entry per real argument."""
    source, build = tmp_path / "proj", tmp_path / "build"
    source.mkdir(parents=True)
    (source / "probe.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")

    stubs = "\n".join(f"function({name})\nendfunction()" for name in STUBBED_COMMANDS)
    stubs += "\n" + "\n".join(f"add_library({name} INTERFACE)" for name in STUBBED_TARGETS)
    body = (
        PROJECT.replace("@stubs@", stubs)
        .replace("@suppressions@", SANITIZED if sanitized else "")
        .replace("@test_module@", TEST_MODULE.as_posix())
        .replace("@flaky@", "FLAKY" if flaky else "")
    )
    (source / "CMakeLists.txt").write_text(body, encoding="utf-8")

    finished = subprocess.run(
        ["cmake", "-S", str(source), "-B", str(build)],
        capture_output=True,
        text=True,
        # Asserted below, so a failure can show what CMake said.
        check=False,
        env={**os.environ, "CMAKE_BUILD_PARALLEL_LEVEL": "1"},
    )
    args = [line.split("ARG|", 1)[1] for line in finished.stdout.splitlines() if "ARG|" in line]
    assert args, finished.stdout + finished.stderr
    return args


def value_of(args: list[str], key: str) -> str:
    """The value paired with `key`, pairing from the start as gtest_discover_tests does.

    Pairing, not searching: under the defect `TIMEOUT` is still in the list, as the value of a
    property named after a sanitizer option, so a search finds it and reports success.
    """
    pairs = dict(zip(args[0::2], args[1::2], strict=False))
    assert key in pairs, f"no property named {key}; the pairs are {pairs}"
    return pairs[key]


@needs_cmake
def test_the_probe_passes_properties_at_all(tmp_path):
    """Passes before and after the fix: it proves the probe reaches the call, not that it is fixed."""
    args = properties(tmp_path, sanitized=True, flaky=False)
    assert "LABELS" in args


@needs_cmake
def test_every_sanitizer_option_stays_in_one_environment_value(tmp_path):
    """Two options must arrive as one argument, or the second becomes a property name."""
    environment = value_of(properties(tmp_path, sanitized=True, flaky=False), "ENVIRONMENT")
    assert "LSAN_OPTIONS" in environment and "ASAN_OPTIONS" in environment, (
        f"ENVIRONMENT carries only one of the two sanitizer options: {environment!r}"
    )


@needs_cmake
def test_extra_properties_survive_a_sanitized_build(tmp_path):
    """The case that was costing the kernel suite its timeout under ASan."""
    args = properties(tmp_path, sanitized=True, flaky=False)
    assert value_of(args, "TIMEOUT") == "60", f"EXTRA_PROPERTIES did not survive the expansion: {args}"


@needs_cmake
def test_a_flaky_suite_keeps_both_labels(tmp_path):
    """Latent: no unit suite is FLAKY today, and a lost label would make `-LE flaky` exclude nothing."""
    labels = value_of(properties(tmp_path, sanitized=False, flaky=True), "LABELS")
    assert labels.split(";") == ["unit", "flaky"], f"LABELS did not arrive as one list: {labels!r}"
