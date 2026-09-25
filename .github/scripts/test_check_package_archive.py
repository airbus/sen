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
    "libs/core/include/sen/core/base/hash32.h",
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
        "sen-0.6.0-x86_64-linux-gnu-12.4.0-relwithdebinfo",
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


WINDOWS_DEBUG_STEM = "sen-0.6.0-amd64-windows-msvc-19.44.0-relwithdebinfo"


def test_a_windows_debug_archive_without_symbols_is_reported(tmp_path):
    """An archive can satisfy every other entry and still have nothing to debug with.

    MSVC keeps debug information outside the binary, so the symbols are a separate file.
    """
    problems = check_archive(write_archive(tmp_path, WINDOWS_DEBUG_STEM, WINDOWS_MEMBERS, ".zip"))
    assert any(".pdb" in problem for problem in problems), problems


def test_a_windows_debug_archive_with_symbols_passes(tmp_path):
    """The same archive carrying a .pdb beside the binary is complete."""
    members = WINDOWS_MEMBERS + ("bin/sen.pdb",)
    assert check_archive(write_archive(tmp_path, WINDOWS_DEBUG_STEM, members, ".zip")) == []


def test_symbols_are_only_required_where_they_live_outside_the_binary(tmp_path):
    """Linux embeds them, so the same archive without a .pdb is complete there."""
    stem = "sen-0.6.0-x86_64-linux-gnu-12.4.0-relwithdebinfo"
    assert check_archive(write_archive(tmp_path, stem, LINUX_MEMBERS)) == []


SYMBOLS_STEM = "sen-0.7.0-rc1-x86_64-linux-gnu-12.4.0-release-symbols"
WINDOWS_SYMBOLS_STEM = "sen-0.7.0-rc1-amd64-windows-msvc-19.44.0-release-symbols"

# What the split produces: debug files named by build id, under the layout gdb searches.
SYMBOLS_MEMBERS = (
    "lib/debug/.build-id/51/58f1b0c6173b8f111723b3249990f8bd51ddc6.debug",
    "lib/debug/.build-id/54/0aee9a88d3f2be1f2dbd93a0f77a5a0e18b1c2.debug",
)


def test_a_symbols_archive_passes(tmp_path):
    """It holds debug information and no program, so the usual entries do not apply."""
    assert check_archive(write_archive(tmp_path, SYMBOLS_STEM, SYMBOLS_MEMBERS)) == []


def test_a_windows_symbols_archive_passes(tmp_path):
    """The same archive on Windows carries .pdb files instead."""
    archive = write_archive(tmp_path, WINDOWS_SYMBOLS_STEM, ("bin/core.pdb", "bin/sen.pdb"), ".zip")
    assert check_archive(archive) == []


def test_a_symbols_archive_carrying_no_symbols_is_rejected(tmp_path):
    """The gap this closes: packaging an empty symbols directory would otherwise pass."""
    problems = check_archive(write_archive(tmp_path, SYMBOLS_STEM, ("lib/debug/README",)))
    assert problems == ["missing entry: debug information (no .debug or .pdb entry)"]


def test_a_symbols_archive_is_not_asked_for_a_program(tmp_path):
    """Applying the release checks to it would reject every valid symbols archive."""
    problems = check_archive(write_archive(tmp_path, SYMBOLS_STEM, SYMBOLS_MEMBERS))
    assert not any("bin/sen" in problem or "shared library" in problem for problem in problems)


def test_a_handler_program_in_the_archive_is_rejected(tmp_path):
    """The kernel forks itself to run the handler, so shipping the program is a defect."""
    members = LINUX_MEMBERS + ("bin/crashpad_handler",)
    problems = check_archive(write_archive(tmp_path, LINUX_NAME, members))
    assert problems == ["must not ship: bin/crashpad_handler"]


def test_the_windows_spelling_is_rejected_too(tmp_path):
    """An .exe suffix is the same program, and the check would otherwise see a new name."""
    members = WINDOWS_MEMBERS + ("bin/crashpad_handler.exe",)
    problems = check_archive(write_archive(tmp_path, WINDOWS_NAME, members, ".zip"))
    assert problems == ["must not ship: bin/crashpad_handler.exe"]


def test_a_name_that_merely_contains_it_is_left_alone(tmp_path):
    """Matching a substring here would reject files the package is supposed to hold."""
    members = LINUX_MEMBERS + ("bin/crashpad_handler_wrapper", "lib/libcrashpad_handler.so")
    assert check_archive(write_archive(tmp_path, LINUX_NAME, members)) == []


def test_an_archive_without_one_says_nothing(tmp_path):
    """The other half: the check has to stay quiet on the archives we actually ship."""
    assert check_archive(write_archive(tmp_path, LINUX_NAME, LINUX_MEMBERS)) == []
