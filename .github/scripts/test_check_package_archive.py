# === test_check_package_archive.py ====================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins what the package check accepts and rejects.

The archive contents here are written out literally rather than derived from
the checker's own constants: a fixture built from REQUIRED_FILES cannot
disagree with it, and would pass even if an entry were dropped.
"""

import io
import os
import tarfile
import zipfile
from pathlib import Path

import pytest
from check_package_archive import (
    REQUIRED_DIRECTORIES,
    REQUIRED_FILES,
    check_archive,
    forbidden_entries,
    unpack_and_run,
)

LINUX_NAME = "sen-0.6.0-x86_64-linux-gnu-12.4.0-release"
WINDOWS_NAME = "sen-0.6.0-amd64-windows-msvc-19.44.35228.0-release"

# Third-party shared objects install(RUNTIME_DEPENDENCY_SET) resolves from the binaries that link
# them. POSIX only: that rule is not enrolled on Windows, so a correct Windows archive holds none.
POSIX_ONLY_MEMBERS = ("lib/libSDL2-2.0.so.0",)

# The layout of a real archive, as built by CPack (verified against one).
LINUX_MEMBERS = (
    "share/doc/sen/LICENSE.txt",
    "bin/sen",
    "bin/cli_gen",
    "lib/libcore.so.0.0.0",
    "lib/libshell.so",
    "lib/libexplorer.so",
    # The py component's library, which is what makes the module entry apply: both are behind
    # SEN_BUILD_PY, and the check asks for the module only when the component shipped.
    "lib/libpy.so",
    "lib/sen_db_python.so",
    "lib/cmake/sen/sen-config.cmake",
    "cmake/sen/sen-config.cmake",
    "lib/cmake/sen/Findspdlog.cmake",
    "cmake/sen/sen-config-version.cmake",
    "lib/cmake/sen/sen_targets.cmake",
    "lib/cmake/sen/sen-config-version.cmake",
    "lib/cmake/sen/util/sen_utils.cmake",
    "lib/cmake/sen/util/exportable-config-compat.cmake.in",
    "lib/cmake/sen/util/exportable-config-version-compat.cmake.in",
    "third_party/include/spdlog/spdlog.h",
    "third_party/include/fmt/format.h",
    "include/sen/core/base/hash32.h",
    "share/sen/resources/syntax_highlighting/stl.tmLanguage.json",
    "share/sen/interfaces/stl/sen/db/db.stl",
    "share/sen/schemas/ether.json",
    "include/stl/sen/db/db.stl.h",
    "include/sen/kernel/component_api.h",
    "share/doc/sen/foss_licenses/spdlog.txt",
) + POSIX_ONLY_MEMBERS

# Windows spells the same archive differently: an .exe, and DLLs -- which are runtime
# artefacts, so they ship beside the executables rather than in the library directory.
WINDOWS_SPELLINGS = {
    "bin/sen": "bin/sen.exe",
    "bin/cli_gen": "bin/cli_gen.exe",
    "lib/libcore.so.0.0.0": "bin/core.dll",
    "lib/libshell.so": "bin/shell.dll",
    "lib/libexplorer.so": "bin/explorer.dll",
    "lib/libpy.so": "bin/py.dll",
    "lib/sen_db_python.so": "bin/sen_db_python.pyd",
}

WINDOWS_MEMBERS = tuple(
    WINDOWS_SPELLINGS.get(member, member) for member in LINUX_MEMBERS if member not in POSIX_ONLY_MEMBERS
)


def write_archive(directory: Path, stem: str, members, suffix: str = ".tar.gz") -> Path:
    """Writes an archive holding the given files under one top-level directory."""
    payload = directory / "payload"
    payload.write_text("x", encoding="utf-8")

    archive = directory / f"{stem}{suffix}"
    if suffix == ".zip":
        with zipfile.ZipFile(archive, "w") as zip_file:
            for member in members:
                zip_file.write(payload, f"{stem}/{member}")
    else:
        with tarfile.open(archive, "w:gz") as tar:
            for member in members:
                tar.add(payload, f"{stem}/{member}")

    return archive


def test_realistic_linux_archive_passes(tmp_path):
    """The layout CPack produces on Linux reports nothing."""
    assert check_archive(write_archive(tmp_path, LINUX_NAME, LINUX_MEMBERS)) == []


def test_realistic_windows_archive_passes(tmp_path):
    """The Windows zip passes, where the executable carries the .exe suffix."""
    archive = write_archive(tmp_path, WINDOWS_NAME, WINDOWS_MEMBERS, suffix=".zip")
    assert check_archive(archive) == []


@pytest.mark.parametrize("dropped", REQUIRED_FILES)
def test_each_required_file_is_pinned(tmp_path, dropped):
    """Dropping any required file is reported, not just one of them."""
    members = [member for member in LINUX_MEMBERS if member != dropped]
    assert check_archive(write_archive(tmp_path, LINUX_NAME, members)) == [f"missing entry: {dropped}"]


@pytest.mark.parametrize("dropped", REQUIRED_DIRECTORIES)
def test_each_required_directory_is_pinned(tmp_path, dropped):
    """Dropping the contents of any required directory is reported."""
    members = [member for member in LINUX_MEMBERS if not member.startswith(f"{dropped}/")]
    assert check_archive(write_archive(tmp_path, LINUX_NAME, members)) == [f"missing entry: {dropped}"]


def test_an_empty_directory_does_not_ship_a_file(tmp_path):
    """A directory named like a required file does not satisfy it."""
    members = [member for member in LINUX_MEMBERS if member != "bin/sen"] + ["bin/sen/placeholder"]
    assert check_archive(write_archive(tmp_path, LINUX_NAME, members)) == ["missing entry: bin/sen"]


def test_unexpected_name_is_reported(tmp_path):
    """A drifting archive name would leave the release without artifacts."""
    problems = check_archive(write_archive(tmp_path, "sen-package", LINUX_MEMBERS))
    assert problems == ["name does not match the expected pattern: sen-package.tar.gz"]


@pytest.mark.parametrize(
    "stem",
    [
        "sen-0.6.0-rc1-x86_64-linux-gnu-12.4.0-release",
        "sen-latest-aarch64-linux-gnu-12.3.0-release",
        "sen-0.6.0-x86_64-linux-gnu-12.4.0-debug",
    ],
)
def test_names_cpack_really_produces_are_accepted(tmp_path, stem):
    """Release candidates, untagged builds and Debug archives are all valid names."""
    assert check_archive(write_archive(tmp_path, stem, LINUX_MEMBERS)) == []


def test_empty_archive_is_reported(tmp_path):
    """An archive with no members fails with a clear message, not a confusing one."""
    archive = tmp_path / f"{LINUX_NAME}.tar.gz"
    with tarfile.open(archive, "w:gz"):
        pass

    with pytest.raises(SystemExit, match="no entries"):
        check_archive(archive)


def test_a_symlinked_executable_ships(tmp_path):
    """The sen executable and the versioned libraries ship as symlinks."""
    payload = tmp_path / "payload"
    payload.write_text("x", encoding="utf-8")

    archive = tmp_path / f"{LINUX_NAME}.tar.gz"
    with tarfile.open(archive, "w:gz") as tar:
        for member in LINUX_MEMBERS:
            if member == "bin/sen":
                link = tarfile.TarInfo(f"{LINUX_NAME}/{member}")
                link.type = tarfile.SYMTYPE
                link.linkname = "cli_sen"
                tar.addfile(link)
            else:
                tar.add(payload, f"{LINUX_NAME}/{member}")

    assert check_archive(archive) == []


def test_archive_without_shared_libraries_is_rejected(tmp_path):
    """The gap this check closes: everything else present, no library shipped."""
    # lib/lib, not lib/ -- the library directory now also holds the package config, and stripping
    # the whole of it would remove five required files and stop this isolating what it names.
    members = tuple(m for m in LINUX_MEMBERS if not m.startswith("lib/lib"))
    problems = check_archive(write_archive(tmp_path, LINUX_NAME, members))
    assert any("shared library" in problem for problem in problems)


def test_versioned_and_bare_library_names_both_satisfy(tmp_path):
    """libcore.so is a symlink to libcore.so.0.0.0; either spelling is the library."""
    for spelling in ("lib/libcore.so", "lib/libcore.so.0.0.0"):
        members = tuple(m for m in LINUX_MEMBERS if not m.startswith("lib/libcore")) + (spelling,)
        assert check_archive(write_archive(tmp_path, LINUX_NAME, members)) == []


def test_windows_and_macos_library_spellings_are_accepted(tmp_path):
    """The name has no prefix or extension precisely because these three differ."""
    for spelling in ("bin/core.dll", "lib/libcore.dylib"):
        members = tuple(m for m in WINDOWS_MEMBERS if not m.startswith("bin/core")) + (spelling,)
        assert check_archive(write_archive(tmp_path, WINDOWS_NAME, members, ".zip")) == []


def test_a_library_outside_the_library_directory_does_not_count(tmp_path):
    """A .so beside the executables is where they used to live, and is no longer the answer."""
    members = tuple(m for m in LINUX_MEMBERS if not m.startswith("lib/libcore")) + ("bin/libcore.so",)
    problems = check_archive(write_archive(tmp_path, LINUX_NAME, members))
    assert any("shared library" in problem for problem in problems)


def test_a_linux_archive_missing_a_vendored_third_party_library_is_rejected(tmp_path):
    """The runtime dependency set's silent failure is resolving nothing, which every other entry survives."""
    members = tuple(member for member in LINUX_MEMBERS if member not in POSIX_ONLY_MEMBERS)
    assert check_archive(write_archive(tmp_path, LINUX_NAME, members)) == [
        "missing entry: lib/libSDL2* (shared library)"
    ]


def test_a_windows_archive_is_not_required_to_ship_them(tmp_path):
    """The other half of the control: the same absence must be correct on Windows, or the check is wrong."""
    archive = write_archive(tmp_path, WINDOWS_NAME, WINDOWS_MEMBERS, suffix=".zip")
    assert not any("libSDL2" in member for member in WINDOWS_MEMBERS)
    assert check_archive(archive) == []


def test_a_library_the_target_system_provides_is_rejected(tmp_path):
    """The upper bound: the exclusion list bounds what is copied by directory, which bounds nothing."""
    members = LINUX_MEMBERS + ("lib/libpython3.10.so.1.0",)
    assert check_archive(write_archive(tmp_path, LINUX_NAME, members)) == [
        "the target system provides this, it must not ship: lib/libpython3.10.so.1.0"
    ]


def test_sen_own_libraries_are_not_mistaken_for_system_ones(tmp_path):
    """The other half of the control, and the reason the prefixes carry punctuation.

    libcore, libmisc and libpy would all match a prefix spelled "libc", "libm" or "libpython"
    carelessly, and the check would reject a correct archive.
    """
    sen_libraries = ["lib/libcore.so.0.0.0", "lib/libutil.so", "lib/libpy.so", "lib/libpy_interface.so"]
    assert forbidden_entries(sen_libraries) == []
    assert check_archive(write_archive(tmp_path, LINUX_NAME, LINUX_MEMBERS)) == []


def test_the_python_module_must_ship_in_the_directory_its_loader_searches(tmp_path):
    """POSIX resolves it from the library directory; Windows only from the module's own directory."""
    members = tuple(m for m in LINUX_MEMBERS if "sen_db_python" not in m)
    assert check_archive(write_archive(tmp_path, LINUX_NAME, members)) == [
        "missing entry: lib/sen_db_python* (python extension module)"
    ]
    windows = tuple(m for m in WINDOWS_MEMBERS if "sen_db_python" not in m)
    assert check_archive(write_archive(tmp_path, WINDOWS_NAME, windows, suffix=".zip")) == [
        "missing entry: bin/sen_db_python* (python extension module)"
    ]


def test_the_python_module_in_the_wrong_directory_is_rejected(tmp_path):
    """The other half: present, but where the platform's loader will not look."""
    members = tuple("bin/sen_db_python.so" if "sen_db_python" in m else m for m in LINUX_MEMBERS)
    assert check_archive(write_archive(tmp_path, LINUX_NAME, members)) == [
        "missing entry: lib/sen_db_python* (python extension module)"
    ]


def _archive_with_executable(tmp_path, body: str):
    """Builds a real archive whose bin/sen is a script, so unpack_and_run has something to start."""
    root = tmp_path / LINUX_NAME
    (root / "bin").mkdir(parents=True)
    executable = root / "bin" / ("sen.exe" if os.name == "nt" else "sen")
    executable.write_text(body)
    executable.chmod(0o755)
    archive = tmp_path / f"{LINUX_NAME}.tar.gz"
    with tarfile.open(archive, "w:gz") as tar:
        tar.add(root, arcname=LINUX_NAME)
    return archive


@pytest.mark.skipif(os.name == "nt", reason="the probe executable is a POSIX shell script")
def test_unpack_and_run_reports_an_executable_that_cannot_start(tmp_path):
    """The failure this exists for: every file present, and the loader cannot resolve them."""
    archive = _archive_with_executable(tmp_path, "#!/bin/sh\necho 'not found' >&2\nexit 127\n")
    problems = unpack_and_run(archive)
    assert len(problems) == 1
    assert problems[0].startswith("the shipped executable did not run")


@pytest.mark.skipif(os.name == "nt", reason="the probe executable is a POSIX shell script")
def test_unpack_and_run_accepts_an_executable_that_starts(tmp_path):
    """The other half, or the test above would pass against a check that always fails."""
    assert unpack_and_run(_archive_with_executable(tmp_path, "#!/bin/sh\necho 0.0.0\n")) == []


@pytest.mark.skipif(os.name == "nt", reason="the probe executable is a POSIX shell script")
def test_unpack_and_run_reports_an_archive_with_no_executable(tmp_path):
    """A silent pass here would make --execute a no-op on an archive that shipped no binary."""
    root = tmp_path / LINUX_NAME
    (root / "lib").mkdir(parents=True)
    (root / "lib" / "libcore.so").write_text("")
    archive = tmp_path / f"{LINUX_NAME}.tar.gz"
    with tarfile.open(archive, "w:gz") as tar:
        tar.add(root, arcname=LINUX_NAME)
    assert unpack_and_run(archive) == ["no executable to run at bin/sen"]


@pytest.mark.skipif(os.name == "nt", reason="builds a POSIX symlink member")
def test_unpack_and_run_refuses_a_member_that_escapes_the_destination(tmp_path):
    """This script is a CLI someone points at a downloaded release, and --execute then runs it."""
    archive = tmp_path / f"{LINUX_NAME}.tar.gz"
    escape_target = tmp_path / "OUTSIDE"
    with tarfile.open(archive, "w:gz") as tar:
        directory = tarfile.TarInfo(LINUX_NAME)
        directory.type = tarfile.DIRTYPE
        directory.mode = 0o755
        tar.addfile(directory)
        link = tarfile.TarInfo(f"{LINUX_NAME}/escape")
        link.type = tarfile.SYMTYPE
        link.linkname = str(escape_target)
        tar.addfile(link)
        payload = b"escaped\n"
        member = tarfile.TarInfo(f"{LINUX_NAME}/escape/x")
        member.size = len(payload)
        tar.addfile(member, io.BytesIO(payload))

    problems = unpack_and_run(archive)
    assert len(problems) == 1
    assert problems[0].startswith("the archive could not be unpacked")
    assert not escape_target.exists()


def test_unpack_and_run_reports_a_truncated_archive(tmp_path):
    """A half-downloaded release should read as a verdict, not a traceback."""
    archive = tmp_path / f"{LINUX_NAME}.tar.gz"
    archive.write_bytes(b"\x1f\x8b\x08\x00truncated")
    problems = unpack_and_run(archive)
    assert len(problems) == 1
    assert problems[0].startswith("the archive could not be unpacked")


def test_the_python_module_is_not_required_when_the_component_did_not_ship(tmp_path):
    """The other half: an archive built without SEN_BUILD_PY has neither, and that is correct."""
    members = tuple(m for m in LINUX_MEMBERS if "libpy.so" not in m and "sen_db_python" not in m)
    assert check_archive(write_archive(tmp_path, LINUX_NAME, members)) == []
