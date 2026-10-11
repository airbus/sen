# === test_stl_imports_are_in_the_build_graph.py =======================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that generated output depends on every model it was built from.

A `.stl` reaches its imports through the `-i` search path while the generator runs, so `DEPENDS`
cannot name them: the build system learns about them from a depfile the generator writes as it
reads, or not at all. Without one, editing an imported model left the importing package's generated
header stale, and the symptom hid itself -- header scanning recompiles the `.cpp` anyway, so the
build still looks like it responded, and a clean build makes it disappear.

Three separate places needed it and two were easy to miss: the code generation command, the schema
command beside it, and the FOM directory glob, whose list becomes a custom command's `OUTPUT` rather
than its `DEPENDS`, so a file added after configuring gets no generation rule at all.

These read the generated build graph rather than the CMake text. A `DEPFILE` keyword present in the
file but never passed to the generator satisfies a grep and still leaves the edge empty.
"""

import os
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
CODEGEN_MODULE = ROOT / "cmake" / "util" / "sen_codegen_utils.cmake"

# The module leans on these; stubbing them keeps the module itself real, which is the point.
STUBBED_COMMANDS = ("sen_skip_linting", "sen_configure_target", "add_properties_to_export_set")
STUBBED_TARGETS = ("core", "kernel", "db", "util", "gen")

PREAMBLE = """
cmake_minimum_required(VERSION 3.21)
project(graph_probe CXX)

@stubs@

# Not stub_gen: that would collide with the sen::gen stub above.
add_executable(probe_gen stub_gen.cpp)
add_executable(sen::cli_gen ALIAS probe_gen)

include(@codegen_module@)

add_library(probe_lib STATIC probe.cpp)
"""

STL_CALL = """
sen_generate_code(
  TARGET probe_lib
  LANG cpp
  BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/stl
  STL_FILES ${CMAKE_CURRENT_SOURCE_DIR}/stl/leaf.stl
  SCHEMA_FILE ${CMAKE_CURRENT_BINARY_DIR}/probe_schema.json
)
"""

FOM_CALL = """
sen_generate_code(
  TARGET probe_lib
  LANG cpp
  HLA_FOM_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/fom_one ${CMAKE_CURRENT_SOURCE_DIR}/fom_two
)
"""

BASE_STL = """package probe;

struct Base
{
  value : u32
}
"""

LEAF_STL = """import "base.stl"

package probe;

struct Leaf
{
  base : probe.Base
}
"""

FOM_XML = '<?xml version="1.0"?><objectModel/>\n'


def ninja() -> str | None:
    """Ninja, which every Sen profile pins as the CMake generator."""
    return os.environ.get("SEN_TEST_NINJA") or shutil.which("ninja")


needs_cmake = pytest.mark.skipif(shutil.which("cmake") is None or ninja() is None, reason="needs cmake and ninja")


def write_project(source: Path, call: str) -> None:
    """Lays out a project that calls Sen's real codegen module."""
    source.mkdir(parents=True, exist_ok=True)
    (source / "stub_gen.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
    (source / "probe.cpp").write_text("int probeDummy() { return 0; }\n", encoding="utf-8")

    stubs = "\n".join(f"function({name})\nendfunction()" for name in STUBBED_COMMANDS)
    stubs += "\n" + "\n".join(
        f"add_library(stub_{name} INTERFACE)\nadd_library(sen::{name} ALIAS stub_{name})" for name in STUBBED_TARGETS
    )
    body = PREAMBLE.replace("@stubs@", stubs).replace("@codegen_module@", CODEGEN_MODULE.as_posix()) + call
    (source / "CMakeLists.txt").write_text(body, encoding="utf-8")


def run_cmake(source: Path, build: Path) -> None:
    """Configures, asserting on the status so a failure shows what CMake said."""
    finished = subprocess.run(
        [
            "cmake",
            "-S",
            str(source),
            "-B",
            str(build),
            "-G",
            "Ninja",
            f"-DCMAKE_MAKE_PROGRAM={ninja()}",
            "-DCMAKE_BUILD_TYPE=Release",
        ],
        capture_output=True,
        text=True,
        check=False,
    )
    assert finished.returncode == 0, finished.stdout + finished.stderr


def configure(tmp_path: Path) -> str:
    """A project generating from one model that imports another, returning the build graph."""
    source, build = tmp_path / "proj", tmp_path / "build"
    (source / "stl").mkdir(parents=True)
    (source / "stl" / "base.stl").write_text(BASE_STL, encoding="utf-8")
    (source / "stl" / "leaf.stl").write_text(LEAF_STL, encoding="utf-8")
    write_project(source, STL_CALL)
    run_cmake(source, build)
    return (build / "build.ninja").read_text(encoding="utf-8")


def configure_fom(tmp_path: Path) -> str:
    """Two FOM directories, because what is under test is that both reach the graph."""
    source, build = tmp_path / "proj", tmp_path / "build"
    for directory, name in (("fom_one", "first.xml"), ("fom_two", "second.xml")):
        (source / directory).mkdir(parents=True)
        (source / directory / name).write_text(FOM_XML, encoding="utf-8")
    write_project(source, FOM_CALL)
    run_cmake(source, build)
    return (build / "build.ninja").read_text(encoding="utf-8")


def generation_edge(graph: str, marker: str) -> str:
    """The build statement producing `marker`, with only its own attribute lines.

    Matched on the statement's own line and closed at the first unindented line after it. A
    looser boundary runs past the statement into the comment that introduces the *next* edge,
    which made a neighbouring edge match this one's marker.
    """
    lines = graph.splitlines()
    starts = [
        i for i, line in enumerate(lines) if line.startswith("build ") and marker in line and "CUSTOM_COMMAND" in line
    ]
    assert len(starts) == 1, f"expected one custom command producing {marker}, found {len(starts)}"
    end = starts[0] + 1
    while end < len(lines) and lines[end].startswith(" "):
        end += 1
    return "\n".join(lines[starts[0] : end])


def edge_inputs(graph: str, marker: str) -> list[str]:
    """The explicit inputs of the command producing `marker`, order-only ones excluded.

    Inputs specifically: a generated file is named after the model it came from -- `first.xml.h`
    from `first.xml` -- so searching a whole graph for a model's name finds it as part of an
    *output* whether or not anything depends on it. That is how the first version of these
    checks passed with the defect still present.
    """
    after_rule = generation_edge(graph, marker).splitlines()[0].split(" CUSTOM_COMMAND ", 1)[1]
    return after_rule.split(" || ", 1)[0].split()


@needs_cmake
def test_the_probe_generates_an_edge_at_all(tmp_path):
    """Passes with or without the fixes: it proves the harness works, not that a defect is gone."""
    assert "leaf.stl" in generation_edge(configure(tmp_path), "leaf.stl.h")


@needs_cmake
def test_the_generation_edge_carries_a_depfile(tmp_path):
    """The only route an import has into the build graph."""
    edge = generation_edge(configure(tmp_path), "leaf.stl.h")
    assert "depfile = " in edge, "the generation edge declares no depfile: an import cannot reach the build graph"


@needs_cmake
def test_the_generator_is_asked_to_write_one(tmp_path):
    """A DEPFILE the generator is never told to write names a file that never appears."""
    edge = generation_edge(configure(tmp_path), "leaf.stl.h")
    assert "--depfile" in edge, "the generator is not passed --depfile, so the declared depfile stays absent"


@needs_cmake
def test_the_schema_edge_carries_a_depfile(tmp_path):
    """The schema parses the same models, so it depends on their imports too.

    A separate custom command from the generation one, and it was missed the first time
    precisely because fixing the obvious one looks like finishing the job.
    """
    edge = generation_edge(configure(tmp_path), "probe_schema.json")
    assert "depfile = " in edge, "the schema edge declares no depfile: its imports cannot reach the graph"
    assert "--depfile" in edge, "the generator is not asked to write the schema depfile"


@needs_cmake
def test_one_fom_directory_reaches_the_graph(tmp_path):
    """Passes with or without the fix: it proves the FOM harness works."""
    inputs = edge_inputs(configure_fom(tmp_path), "first.xml.h")
    assert any(name.endswith("second.xml") for name in inputs), "the FOM probe declared no xml dependency at all"


@needs_cmake
def test_every_fom_directory_reaches_the_graph(tmp_path):
    """A glob into one shared name left every directory but the last out of DEPENDS."""
    inputs = edge_inputs(configure_fom(tmp_path), "first.xml.h")
    assert any(name.endswith("first.xml") for name in inputs), (
        "the first FOM directory's xml is not a dependency: editing it would rebuild nothing"
    )


@needs_cmake
def test_a_fom_file_added_after_configuring_gets_a_generation_rule(tmp_path):
    """A FOM directory is named, not its files, so dropping one in touches nothing CMake watches.

    The globbed list becomes the custom command's OUTPUT, so without CONFIGURE_DEPENDS a new
    file has no generation rule at all -- no stale output, no diagnostic, nothing.
    """
    configure_fom(tmp_path)
    source, build = tmp_path / "proj", tmp_path / "build"
    (source / "fom_one" / "late.xml").write_text(FOM_XML, encoding="utf-8")

    # Building re-runs CMake when a watched directory listing changed; the stub generator makes
    # that cheap, and the regenerated graph is what the assertion reads.
    subprocess.run([ninja(), "-C", str(build), "build.ninja"], capture_output=True, text=True, check=False)
    graph = (build / "build.ninja").read_text(encoding="utf-8")
    assert "late.xml" in graph, "a FOM file added after configuring has no generation rule"
