# === test_record_environment.py =======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""What the environment record says about the machine a run happened on."""

import sys

from record_environment import compiler_from_build, first_line


def write_compiler_record(build_dir, body):
    """Writes a CMakeCXXCompiler.cmake where CMake puts it, and returns the build directory."""
    where = build_dir / "CMakeFiles" / "4.3.1"
    where.mkdir(parents=True)
    (where / "CMakeCXXCompiler.cmake").write_text(body, encoding="utf-8")
    return build_dir


def test_the_compiler_version_comes_from_the_build_when_the_tool_cannot_be_asked(tmp_path):
    """The build tree answers when the compiler itself cannot be run.

    MSVC puts cl on PATH only inside a developer shell, so it could not be asked for its version
    and was recorded as its own name. CMake established the version at configure time.
    """
    body = 'set(CMAKE_CXX_COMPILER_ID "MSVC")\nset(CMAKE_CXX_COMPILER_VERSION "19.44.35207")\n'
    assert compiler_from_build(write_compiler_record(tmp_path, body)) == "MSVC 19.44.35207"


def test_a_build_tree_with_no_compiler_record_yields_nothing(tmp_path):
    """Nothing to say is said as nothing, not as a wrong guess."""
    assert not compiler_from_build(tmp_path)
    assert not compiler_from_build(None)


def test_a_tool_that_prints_its_version_on_stderr_is_still_read():
    """MSVC writes its banner to stderr; a reader of stdout alone records no version at all."""
    said = first_line((sys.executable, "-c", "import sys; print('v 1.2.3', file=sys.stderr)"))
    assert said == "v 1.2.3"


def test_stdout_wins_where_a_tool_writes_to_both():
    """The banner is on stdout for every tool that has one, and that stays the answer."""
    said = first_line((sys.executable, "-c", "import sys; print('out 1.0'); print('err 2.0', file=sys.stderr)"))
    assert said == "out 1.0"
