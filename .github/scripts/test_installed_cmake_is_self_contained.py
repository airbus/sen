# === test_installed_cmake_is_self_contained.py ========================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks what the cmake files Sen installs must not do merely by being included.

`sen-config.cmake` includes them, so whatever sits at their file scope runs in every consumer's
build. 0.7.0 added `REQUIRED` to the clang-tidy lookup in `sen_utils.cmake`, which turned a missing
development tool into a failed configure for anyone consuming Sen; 0.6.0 had the same lookup without
it and carried on. Nothing caught it, because the conan test package runs in an image where
clang-tidy is installed and the requirement was always satisfied.

The two rules here are narrow on purpose, and neither covers everything:

- A tool a consumer needs only when they call a function belongs inside that function, not at file
  scope with `REQUIRED`.
- A variable that changes how a consumer's code is compiled must not be set by inclusion.

What they do NOT cover: an option added to `sen_configure_target`, which runs on a consumer's target
through `add_sen_package`. That is guarded on the consumer side instead, by the assertions in
`.conan/test_packages/package/misc_functions/sen_configure_target/CMakeLists.txt`, because only a
real consumer build can see the resulting property.
"""

import re
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
INSTALL = ROOT / "cmake" / "util" / "install.cmake"

# The compiler-launcher block predates these rules and is deliberately kept: a consumer with ccache
# on PATH has their build routed through it. Recorded as an accepted exception rather than removed,
# so that a *second* one cannot arrive unnoticed. Keyed by file and variable, so moving it fails here.
ACCEPTED_GLOBAL_WRITES = {("sen_utils.cmake", "CMAKE_${lang}_COMPILER_LAUNCHER")}

BODY_OPEN = re.compile(r"^\s*(function|macro)\s*\(", re.IGNORECASE)
BODY_CLOSE = re.compile(r"^\s*(endfunction|endmacro)\s*\(", re.IGNORECASE)
# find_package is deliberately absent: a -config.cmake declaring its own dependency REQUIRED is
# the normal idiom and is not the defect. The defect is demanding a development *tool*.
FIND_CALL = re.compile(r"^\s*find_(program|library|path|file)\s*\(", re.IGNORECASE)
GLOBAL_WRITE = re.compile(r"^\s*set\s*\(\s*(CMAKE_[A-Za-z0-9_${}]*)", re.IGNORECASE)


# A message() at file scope, which is where a FATAL_ERROR aborts a consumer's configure over what
# they happen to have installed. Inside a function it fires only when that function is misused.
MESSAGE_CALL = re.compile(r"^\s*message\s*\(", re.IGNORECASE)


def strip_noise(line: str) -> str:
    """Removes quoted strings and trailing comments.

    Both have produced wrong answers: `REQUIRED` inside a DOC string read as a requirement, and a
    `)` inside a trailing comment ended a call early so the keyword on the next line was missed.
    """
    out, in_string, i = [], False, 0
    while i < len(line):
        ch = line[i]
        if ch == '"' and (i == 0 or line[i - 1] != "\\"):
            in_string = not in_string
            out.append('"')
        elif ch == "#" and not in_string:
            break
        else:
            out.append(" " if in_string else ch)
        i += 1
    return "".join(out)


def installed_cmake_files() -> list[Path]:
    """The files a consumer executes: what install.cmake copies, plus the config templates."""
    # Every cmake path install.cmake names, not only those written inline in an install(FILES ...):
    # the util files reach it through a variable, and a regex over the install() call alone found
    # four of them and silently missed five.
    text = INSTALL.read_text(encoding="utf-8")
    names: set[Path] = set()
    for token in re.findall(r"\$\{[A-Za-z_]+\}/([A-Za-z0-9_./-]+\.cmake(?:\.in)?)", text):
        names.add(ROOT / token)
    for extra in ("cmake/util/sen-config.cmake.in",):
        names.add(ROOT / extra)
    names.update((ROOT / "cmake" / "util" / "interfaces").glob("*-config.cmake.in"))
    return sorted(p for p in names if p.exists())


def classify(call: str, rule: str, offences: dict[str, list[str]]) -> None:
    """Records a completed file-scope call if it breaks the rule it was opened for."""
    if rule == "required_find" and re.search(r"\bREQUIRED\b", call):
        offences["required_find"].append(" ".join(call.split()))
    elif rule == "file_scope_abort" and re.search(r"\bFATAL_ERROR\b", call):
        offences["file_scope_abort"].append(" ".join(call.split()))


def scan(text: str) -> dict[str, list[str]]:
    """Returns the file-scope offences in a cmake file, by rule."""
    offences: dict[str, list[str]] = {"required_find": [], "global_write": [], "file_scope_abort": []}
    depth, call, in_call, rule = 0, "", False, ""
    for raw in text.splitlines():
        line = strip_noise(raw)
        if in_call:
            call += " " + line.strip()
            if ")" in line:
                in_call = False
                if depth == 0:
                    classify(call, rule, offences)
            continue
        if BODY_OPEN.match(line):
            depth += 1
        elif BODY_CLOSE.match(line):
            depth = max(0, depth - 1)
        elif (opened := FIND_CALL.match(line)) or MESSAGE_CALL.match(line):
            rule = "required_find" if opened else "file_scope_abort"
            call, in_call = line.strip(), ")" not in line
            if not in_call and depth == 0:
                classify(call, rule, offences)
        elif depth == 0 and (m := GLOBAL_WRITE.match(line)):
            offences["global_write"].append(m.group(1))
    return offences


def test_install_list_is_readable():
    """A rename that emptied or shrank this list would leave every assertion below vacuous."""
    files = installed_cmake_files()
    found = {p.name for p in files}
    # Named rather than counted: a count stays green when the list shrinks, which is how a regex
    # that found four of nine went unnoticed until an explicit name was asserted.
    for expected in (
        "sen_utils.cmake",
        "sen_misc_utils.cmake",
        "sen_package_utils.cmake",
        "sen_codegen_utils.cmake",
        "git_info.cmake",
        "sen-config.cmake.in",
    ):
        assert expected in found, f"{expected} is installed but no longer scanned: {sorted(found)}"
    assert any(n.endswith("-config.cmake.in") for n in found)


@pytest.mark.parametrize("path", installed_cmake_files(), ids=lambda p: p.name)
def test_no_required_tool_lookup_at_include_time(path):
    """A consumer's find_package(sen) must not fail because a development tool is absent."""
    offences = scan(path.read_text(encoding="utf-8"))["required_find"]
    assert not offences, f"{path.name} demands a tool of every consumer: {offences}"


@pytest.mark.parametrize("path", installed_cmake_files(), ids=lambda p: p.name)
def test_no_unaccepted_global_write_at_include_time(path):
    """Including a Sen cmake file must not change how a consumer's own code is compiled."""
    offences = [
        v
        for v in scan(path.read_text(encoding="utf-8"))["global_write"]
        if (path.name, v) not in ACCEPTED_GLOBAL_WRITES
    ]
    assert not offences, f"{path.name} sets {offences} on every consumer merely by being included"


@pytest.mark.parametrize("path", installed_cmake_files(), ids=lambda p: p.name)
def test_no_fatal_error_at_file_scope(path):
    """Including a Sen cmake file must not abort over what a consumer happens to have installed.

    A fatal error inside a function fires when that function is misused, which is the author's doing.
    At file scope it fires on a consumer who only called find_package(sen), and there is nothing they
    can pass to get past it. Report the situation and carry on instead.
    """
    offences = scan(path.read_text(encoding="utf-8"))["file_scope_abort"]
    assert not offences, f"{path.name} can abort a consumer's configure: {offences}"


# --- the scanners must be shown to fire, not merely to pass -----------------------------------------

PLANTS = [
    ("find_program(t NAMES clang-tidy REQUIRED)", "required_find", "the one-liner 0.7.0 shipped"),
    ("find_program(\n  t\n  NAMES clang-tidy\n  REQUIRED\n)", "required_find", "wrapped by cmake-format"),
    (
        "find_program(t NAMES clang-tidy) # REQUIRED)\nset(CMAKE_CXX_FLAGS x)",
        "global_write",
        "a ) inside a trailing comment must not end the call early",
    ),
    ("set(CMAKE_CXX_COMPILER_LAUNCHER ccache)", "global_write", "a second compiler launcher"),
    ('message(FATAL_ERROR "spdlog does not match")', "file_scope_abort", "a refusal at file scope"),
    (
        'message(\n  FATAL_ERROR\n    "spdlog does not match"\n)',
        "file_scope_abort",
        "a refusal wrapped by cmake-format",
    ),
    (
        'if(NOT x)\n  message(FATAL_ERROR "no")\nendif()',
        "file_scope_abort",
        "an if() at file scope is still file scope",
    ),
]

QUIET = [
    "find_package_handle_standard_args(spdlog REQUIRED_VARS D)",
    'find_program(t NAMES clang-tidy DOC "REQUIRED for analysis")',
    "function(f)\n  find_program(t NAMES clang-tidy REQUIRED)\nendfunction()",
    "function(f)\n  set(CMAKE_CXX_FLAGS x)\nendfunction()",
    'function(f)\n  message(FATAL_ERROR "used wrongly")\nendfunction()',
    'message(WARNING "spdlog does not match")',
    'message(STATUS "FATAL_ERROR is what we used to do here")',
]


@pytest.mark.parametrize("text,rule,why", PLANTS, ids=[p[2] for p in PLANTS])
def test_scanner_reports_planted_defects(text, rule, why):
    """A scanner that finds nothing passes every file, so each rule is shown to fire."""
    assert scan(text)[rule], f"the scanner is blind to: {why}"


@pytest.mark.parametrize(
    "text",
    QUIET,
    ids=[
        "handle_standard_args",
        "doc_string",
        "inside_function",
        "set_inside_function",
        "fatal_inside_function",
        "a_warning",
        "fatal_error_in_prose",
    ],
)
def test_scanner_stays_quiet_on_legitimate_code(text):
    """The other half: shapes that look like the defect but are not it."""
    found = scan(text)
    assert not found["required_find"] and not found["global_write"] and not found["file_scope_abort"], found
