# === test_build_info_accessors.py =====================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that the generated build_info.cpp defines every accessor component.h declares.

`component.h` only declares them; the definitions come from the file `sen_add_build_info` generates
for each target. Declaring one the template does not define is an undefined reference in every
package that gets built, and the error names a mangled symbol rather than the template that owes it.

The compiler catches a changed signature by itself, in the generated file, so this looks only at
which accessors each side names.
"""

import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "libs" / "kernel" / "include" / "sen" / "kernel" / "component.h"
GENERATED = ROOT / "cmake" / "templates" / "build_info.cpp.in"
EXPORTS_TEMPLATE = ROOT / "libs" / "gen" / "src" / "cpp" / "templates" / "file_package_exports.j2"

# `getGitRef() noexcept`, whether it ends in a semicolon or a body.
ACCESSOR = re.compile(r"\b(get\w+)\s*\(\s*\)\s*noexcept")
# A call through the namespace, which is how the component macro and the generated exports reach them.
CALL = re.compile(r"sen::kernel::(get\w+)\s*\(\s*\)")

# Declared SEN_PRIVATE, which is what marks the ones the generated file owes a definition for.
DECLARED = re.compile(r"SEN_PRIVATE\s+[\w:*&<>\s]*?\b(get\w+)\s*\(\s*\)\s*noexcept\s*;")


def declared_accessors() -> set[str]:
    """The accessors component.h declares without defining."""
    return set(DECLARED.findall(HEADER.read_text(encoding="utf-8")))


def defined_accessors() -> set[str]:
    """The accessors the generated translation unit defines."""
    return set(ACCESSOR.findall(GENERATED.read_text(encoding="utf-8")))


def test_the_generated_file_defines_what_the_header_declares():
    """Either side missing one is an undefined reference in every package that is built."""
    declared, defined = declared_accessors(), defined_accessors()
    assert declared == defined, (
        f"component.h declares {sorted(declared)} but build_info.cpp.in defines {sorted(defined)}. "
        f"Whichever side is short, every package fails to link."
    )


def test_the_accessors_are_not_empty():
    """The comparison above is between sets, and two empty sets are equal."""
    assert len(declared_accessors()) >= 4, f"only found {sorted(declared_accessors())}"


def test_every_accessor_called_is_one_the_header_supplies():
    """The component macro and the generated exports call these; a new one has to be declared."""
    available = set(ACCESSOR.findall(HEADER.read_text(encoding="utf-8")))
    for source in (HEADER, EXPORTS_TEMPLATE):
        called = set(CALL.findall(source.read_text(encoding="utf-8")))
        assert called <= available, (
            f"{source.name} calls {sorted(called - available)}, which component.h does not declare"
        )


@pytest.mark.parametrize(
    ("declared", "defined", "agree"),
    [
        ({"getGitRef", "getBuildTime"}, {"getGitRef", "getBuildTime"}, True),
        ({"getGitRef", "getBuildTime"}, {"getGitRef"}, False),
        ({"getGitRef"}, {"getGitRef", "getBuildTime"}, False),
    ],
    ids=["agree", "template-short", "header-short"],
)
def test_the_comparison_answers_both_ways(declared, defined, agree):
    """Both halves: the check has to reject as well as accept."""
    assert (declared == defined) is agree


def test_the_patterns_read_the_real_files():
    """A pattern that matches nothing would make every test above pass on an empty set."""
    assert "getGitRef" in declared_accessors()
    assert "getGitRef" in defined_accessors()
