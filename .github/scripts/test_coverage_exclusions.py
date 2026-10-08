# === test_coverage_exclusions.py ======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks what the shared coverage exclusions do and do not take out of the denominator.

The list is read by coverage.cmake for gcovr and llvm-cov and by standard_test.yaml for
OpenCppCoverage, so a pattern that is too broad quietly removes hand-written code from every
compiler's figure at once, and one that matches nothing leaves generated code in it. Neither
shows up as a failure anywhere: the number simply moves.
"""

import re
from pathlib import Path

EXCLUSIONS = Path(__file__).resolve().parents[2] / "cmake" / "util" / "coverage-exclusions.txt"

# Written out rather than globbed from the tree: a path that stops existing should fail here
# rather than silently stop being checked.
EXCLUDED = (
    "libs/core/include/sen/core/quantity.stl.h",
    "libs/core/src/quantity.stl.cpp",
    "libs/db/src/schema.xml.cpp",
    "libs/core/generated/sen_types.cpp",
    "libs/core/src/sen_core_build_info.cpp",
    "libs/core/src/sen_exported_types.cpp",
    "components/explorer/src/explorer.cpp",
    "components/shell/src/shell.cpp",
    "components/rest/src/rest.cpp",
    "apps/cli_remote_shell/src/main.cpp",
)

MEASURED = (
    "libs/core/src/quantity.cpp",
    "libs/core/include/sen/core/quantity.h",
    "libs/kernel/src/kernel.cpp",
    "components/term/src/term.cpp",
    "components/jsonrpc/src/jsonrpc.cpp",
    "apps/cli_gen/src/cpp_cli.cpp",
    "apps/cli_archive/main.cpp",
)


def patterns() -> list[str]:
    """The wildcards in the file, comments and blank lines dropped."""
    lines = (line.strip() for line in EXCLUSIONS.read_text(encoding="utf-8").splitlines())
    return [line for line in lines if line and not line.startswith("#")]


def excludes(path: str) -> bool:
    """Applies the file's wildcards the way coverage.cmake translates them."""
    for wildcard in patterns():
        if re.fullmatch(wildcard.replace(".", "[.]").replace("*", ".*"), path):
            return True
    return False


def test_the_list_is_not_empty():
    """An unreadable or renamed file would otherwise excuse every case below."""
    assert len(patterns()) >= 10


def test_generated_and_withdrawn_code_is_left_out():
    """These are the two reasons the list exists."""
    assert [path for path in EXCLUDED if not excludes(path)] == []


def test_hand_written_code_is_measured():
    """A pattern one character too wide takes real code out of every compiler's figure at once."""
    assert [path for path in MEASURED if excludes(path)] == []
