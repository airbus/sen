# === check_package_archive.py =========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that the archive built by CPack is complete.

The release workflow attaches this archive to the GitHub release and the
installer script unpacks it, so its layout is a contract. Without this check a
dropped install rule ships a healthy-looking archive, and a changed archive
name silently leaves the release with no artifacts at all.
"""

import argparse
import re
import sys
import tarfile
import zipfile
from pathlib import Path

# What the archive must hold, as paths under its top-level directory. Keep the
# lists short: one representative of each thing an install rule ships. Files and
# directories are separate, so a directory cannot stand in for a missing file.
REQUIRED_FILES = (
    "LICENSE.txt",
    # sen.exe on Windows; either spelling satisfies this entry.
    "bin/sen",
    "lib/cmake/sen/sen_targets.cmake",
    "lib/cmake/sen/sen-config-version.cmake",
    "lib/cmake/sen/util/sen_utils.cmake",
)

REQUIRED_DIRECTORIES = (
    "libs/core/include/sen/core",
    "resources/syntax_highlighting",
)

# Named without prefix or extension because those differ by platform: libcore.so,
# libcore.dylib, core.dll. Nothing above covers a shared library, so an archive shipping none
# satisfied every entry.
#
# core is linked by the sen executable. shell is a component, and a missing component is the
# silent case: components are opened by name only when a config asks for one. shell ships in
# basic and full; a barebones package has none.
#
# spdlog is the third-party library a consumer links. Without it the find module falls back to
# header-only, so a consumer compiles its own registry and the kernel's logger configuration never
# reaches it. That is a configure warning and nothing afterwards, so the archive is where it is
# caught.
REQUIRED_LIBRARIES = ("core", "shell", "spdlog")

# Defines the installed cmake export has to carry, read out of the file rather than inferred from
# its name. SPDLOG_SHARED_LIB selects which ABI mode spdlog's headers compile against, and on the
# conan path the kernel's exported interface is the only thing that carries it: the find module
# declares it on its vendored branch, which a conan consumer does not reach.
REQUIRED_EXPORT_DEFINES = ("SPDLOG_SHARED_LIB",)

# The export file the defines live in, matched by its tail so that both install layouts answer:
# lib/cmake/sen on main, cmake/sen on release/0.7.x.
EXPORT_FILE = "sen_targets.cmake"

# The kernel links Crashpad's handler library and forks itself to run it, so no separate
# handler program is installed. A test pins that in the build tree; the archive is assembled
# separately, so one could ship the program with every check we own still green.
FORBIDDEN_FILES = ("crashpad_handler",)

# sen-<version>-<processor>-<system>-<compiler>-<version>-<build type>, lower case, with symbols
# in the build type's place for the archive of split debug information. One segment per field:
# install.sh counts them. The version is a tag or "latest", and a tag may carry an -rc suffix.
NAME_PATTERN = re.compile(r"^sen-[^-]+(?:-rc\d+)?-[^-]+-[^-]+-[^-]+-[^-]+-(release|debug|relwithdebinfo|symbols)$")


def list_entries(archive: Path) -> list[str]:
    """Lists the files in the archive, without their common top-level directory.

    Only directory members are dropped, so an install rule that produces an
    empty directory cannot satisfy a required file. Symlinks count: the sen
    executable and the versioned libraries ship as links.
    """
    if archive.suffixes[-2:] == [".tar", ".gz"]:
        with tarfile.open(archive) as tar:
            names = [member.name for member in tar.getmembers() if not member.isdir()]
    elif archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as zip_file:
            names = [name for name in zip_file.namelist() if not name.endswith("/")]
    else:
        raise SystemExit(f"Error: unsupported archive type: {archive.name}")

    if not names:
        raise SystemExit(f"Error: the archive holds no entries: {archive.name}")

    roots = {name.split("/", 1)[0] for name in names}
    if len(roots) != 1:
        raise SystemExit(f"Error: expected one top-level directory, found {sorted(roots)}")

    return [name.split("/", 1)[1] for name in names if "/" in name]


def read_member(archive: Path, entry: str) -> str | None:
    """One member's text, addressed the way `list_entries` reports it, without the top directory."""
    if archive.suffixes[-2:] == [".tar", ".gz"]:
        with tarfile.open(archive) as tar:
            for member in tar.getmembers():
                if member.isfile() and member.name.split("/", 1)[-1] == entry:
                    handle = tar.extractfile(member)
                    return handle.read().decode("utf-8", "replace") if handle else None
    elif archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as zip_file:
            for name in zip_file.namelist():
                if name.split("/", 1)[-1] == entry:
                    return zip_file.read(name).decode("utf-8", "replace")
    return None


def missing_export_defines(archive: Path, entries: list[str]) -> list[str]:
    """Returns the required defines the cmake export does not carry.

    An absent export file is already reported as a missing entry, so this says nothing about it
    rather than reporting the same gap twice.
    """
    exports = [entry for entry in entries if entry.rsplit("/", 1)[-1] == EXPORT_FILE]
    if not exports:
        return []
    text = read_member(archive, exports[0]) or ""
    return [define for define in REQUIRED_EXPORT_DEFINES if define not in text]


def archive_stem(archive: Path) -> str:
    """Returns the archive name without its extension."""
    name = archive.name
    for suffix in (".tar.gz", ".zip"):
        if name.endswith(suffix):
            return name[: -len(suffix)]

    raise SystemExit(f"Error: unsupported archive type: {name}")


def missing_entries(entries: list[str], stem: str = "") -> list[str]:
    """Returns the required files and directories the archive does not hold."""
    if stem.endswith("-symbols"):
        return _missing_symbols(entries)

    present = set(entries)
    missing = [name for name in REQUIRED_FILES if name not in present and f"{name}.exe" not in present]
    missing += [name for name in REQUIRED_DIRECTORIES if not any(entry.startswith(f"{name}/") for entry in entries)]
    missing += [
        f"{name} (shared library)"
        for name in REQUIRED_LIBRARIES
        if not any(_is_shared_library(entry, name) for entry in entries)
    ]
    # MSVC keeps debug information outside the binary, so a Windows archive built for it and
    # shipping no .pdb has nothing to debug with while satisfying every entry above.
    if "relwithdebinfo" in stem and "-windows-" in stem:
        if not any(entry.lower().endswith(".pdb") for entry in entries):
            missing.append("a .pdb (debug information for Windows)")
    return missing


def _missing_symbols(entries: list[str]) -> list[str]:
    """What a symbols archive has to hold, which is nothing the other archives hold.

    It ships debug information and no program, so the checks above would reject every
    valid one. What makes it valid instead is carrying debug information at all: an
    archive that packaged an empty symbols directory would otherwise pass every check
    while being useless, which is the failure this whole change exists to remove.
    """
    if any(entry.endswith(".debug") for entry in entries):
        return []
    if any(entry.lower().endswith(".pdb") for entry in entries):
        return []
    return ["debug information (no .debug or .pdb entry)"]


def _is_shared_library(entry: str, name: str) -> bool:
    """Whether an archive entry is the shared library `name`, where the loader looks for it.

    The directory follows from the spelling rather than the platform: a DLL is a runtime
    artefact and ships beside the executables, while .so and .dylib ship in the library
    directory. A versioned suffix counts, since libcore.so is a symlink to libcore.so.0.0.0.
    """
    directory, _, stem = entry.rpartition("/")
    if stem == f"{name}.dll":
        return directory == "bin"

    if stem == f"lib{name}.dylib" or stem.startswith(f"lib{name}.so"):
        return directory == "lib"

    return False


def check_archive(archive: Path) -> list[str]:
    """Returns the problems found in the archive, empty when it is complete."""
    problems = []
    stem = archive_stem(archive)
    if not NAME_PATTERN.match(stem):
        problems.append(f"name does not match the expected pattern: {archive.name}")

    entries = list_entries(archive)
    problems.extend(f"missing entry: {entry}" for entry in missing_entries(entries, stem))
    problems.extend(f"must not ship: {entry}" for entry in forbidden_entries(entries))
    problems.extend(f"cmake export does not carry {define}" for define in missing_export_defines(archive, entries))
    return problems


def forbidden_entries(entries: list[str]) -> list[str]:
    """Returns the entries that ship something the package is supposed not to contain.

    Absence is the claim here, which is why it needs checking where the claim is made: a
    file that should not exist leaves no trace in any check looking for files that should.
    """
    found = []
    for entry in entries:
        name = entry.rsplit("/", 1)[-1]
        stem = name[: -len(".exe")] if name.lower().endswith(".exe") else name
        if stem in FORBIDDEN_FILES:
            found.append(entry)
    return found


def main() -> int:
    """Checks the archives found in the given build directory."""
    parser = argparse.ArgumentParser(
        prog="check_package_archive",
        description="Checks that the archive built by CPack is complete.",
    )
    parser.add_argument("build_dir", help="Build directory holding the archive that CPack wrote.")
    parser.add_argument(
        "--expect-symbols",
        action="store_true",
        help="Require a symbols archive. Without it the checker validates whatever it finds, so a short set passes.",
    )
    args = parser.parse_args()

    archives = sorted(Path(args.build_dir).glob("sen-*.tar.gz")) + sorted(Path(args.build_dir).glob("sen-*.zip"))
    if not archives:
        raise SystemExit(f"Error: no archive found in {args.build_dir}")

    problems = [problem for archive in archives for problem in check_archive(archive)]
    if args.expect_symbols and not any(archive_stem(archive).endswith("-symbols") for archive in archives):
        problems.append(
            "no symbols archive: the build was asked for debug information and produced "
            f"only {', '.join(archive.name for archive in archives)}"
        )
    for problem in problems:
        print(problem)

    if problems:
        return 1

    print(f"checked {', '.join(archive.name for archive in archives)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
