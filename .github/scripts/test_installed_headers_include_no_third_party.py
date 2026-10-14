#!/usr/bin/env python3
# === test_installed_headers_include_no_third_party.py =================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""Checks the mechanical half of README.md's claim: nothing Sen installs includes a third-party header.

The kernel's API names spdlog's logger and log level and declares them rather than including spdlog,
so a component that never logs compiles neither spdlog nor fmt. Nothing at build time holds that:
adding an include to a public header compiles, and makes the dependency mandatory again for every
consumer.

An allowlist of standard headers rather than a denylist, so a dependency nobody listed still fails.
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# The trees the install rules copy: <prefix>/libs/<name>/include. Generated headers go to the build
# tree instead, so this covers what the repository ships.
HEADER_ROOTS = sorted(ROOT.glob("libs/*/include"))

INCLUDE = re.compile(r"^\s*#\s*include\s*<([^>]+)>", re.MULTILINE)

# The standard library's headers through C++23, plus the C compatibility ones. Anything else in an
# angle include is somebody else's code; one missing from here reads as a third-party include.
STANDARD_HEADERS = frozenset(
    """
    algorithm any array atomic bitset cassert ccomplex cctype cerrno cfenv cfloat charconv chrono
    cinttypes ciso646 climits clocale cmath codecvt compare complex concepts condition_variable
    csetjmp csignal cstdalign cstdarg cstdbool cstddef cstdint cstdio cstdlib cstring ctgmath ctime
    cuchar cwchar cwctype deque exception execution filesystem forward_list fstream functional future
    initializer_list iomanip ios iosfwd iostream istream iterator limits list locale map memory
    memory_resource mutex new numeric optional ostream queue random ratio regex scoped_allocator set
    shared_mutex span sstream stack stdexcept streambuf string string_view strstream system_error
    thread tuple type_traits typeindex typeinfo unordered_map unordered_set utility valarray variant
    vector version
    barrier bit coroutine debugging expected flat_map flat_set format generator hazard_pointer latch
    linalg mdspan numbers print ranges rcu semaphore simd source_location spanstream stacktrace
    stdfloat stop_token syncstream text_encoding
    """.split()
)


def angle_includes(text: str) -> list[str]:
    """Every angle-bracket include in a translation unit, in order."""
    return INCLUDE.findall(text)


def test_the_header_roots_were_found():
    """A glob that matches nothing passes every assertion below, so it is checked first."""
    assert HEADER_ROOTS, "no libs/*/include directories found; the check would pass vacuously"
    assert any(root.glob("**/*.h") for root in HEADER_ROOTS), "no headers under libs/*/include"


def test_no_installed_header_includes_a_third_party_header():
    """The claim itself, over every header the repository installs."""
    offenders = []
    for root in HEADER_ROOTS:
        for header in sorted(root.glob("**/*.h")):
            for include in angle_includes(header.read_text(encoding="utf-8")):
                if include.split("/")[0].removesuffix(".h") not in STANDARD_HEADERS:
                    offenders.append(f"{header.relative_to(ROOT)}: #include <{include}>")
    assert not offenders, "installed headers include third-party headers:\n  " + "\n  ".join(offenders)


def test_the_check_would_catch_one():
    """The detector, against a header it must reject and one it must accept."""
    assert angle_includes("#include <spdlog/spdlog.h>\n") == ["spdlog/spdlog.h"]
    assert angle_includes("#include <memory>\n") == ["memory"]
    assert angle_includes('#include "sen/kernel/component_api.h"\n') == []

    rejected = "spdlog/spdlog.h"
    assert rejected.split("/", maxsplit=1)[0].removesuffix(".h") not in STANDARD_HEADERS
    assert "memory".removesuffix(".h") in STANDARD_HEADERS
