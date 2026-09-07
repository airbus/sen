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

import tarfile
import zipfile
from pathlib import Path

import pytest
from check_package_archive import REQUIRED_DIRECTORIES, REQUIRED_FILES, check_archive

LINUX_NAME = "sen-0.6.0-x86_64-linux-gnu-12.4.0-release"
WINDOWS_NAME = "sen-0.6.0-amd64-windows-msvc-19.44.35228.0-release"

# The layout of a real archive, as built by CPack (verified against one).
LINUX_MEMBERS = (
    "LICENSE.txt",
    "bin/sen",
    "lib/libcore.so.0.0.0",
    "lib/libshell.so",
    "cmake/sen/sen_targets.cmake",
    "cmake/sen/SenConfigVersion.cmake",
    "cmake/sen/util/sen_utils.cmake",
    "include/sen/core/base/hash32.h",
    "resources/syntax_highlighting/stl.tmLanguage.json",
)

# Windows spells the same archive differently: an .exe, and DLLs -- which are runtime
# artefacts, so they ship beside the executables rather than in the library directory.
WINDOWS_SPELLINGS = {
    "bin/sen": "bin/sen.exe",
    "lib/libcore.so.0.0.0": "bin/core.dll",
    "lib/libshell.so": "bin/shell.dll",
}

WINDOWS_MEMBERS = tuple(WINDOWS_SPELLINGS.get(member, member) for member in LINUX_MEMBERS)


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
    members = tuple(m for m in LINUX_MEMBERS if not m.startswith("lib/"))
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
