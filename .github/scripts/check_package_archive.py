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
    # The file find_package actually opens. Its absence was not caught by any entry here, which
    # meant an archive that no consumer could configure against still passed.
    "lib/cmake/sen/sen-config.cmake",
    # The forwarding config for anyone told to point at <prefix>/cmake. This whole change exists
    # to add it, and nothing asserted it shipped.
    "cmake/sen/sen-config.cmake",
    # Removing this makes every consumer's find_package(sen REQUIRED) fail, because sen-config
    # calls find_dependency(spdlog) and this is the only module that resolves it.
    "lib/cmake/sen/Findspdlog.cmake",
    # The version file beside the forwarding config. Without it a versioned find_package through
    # the old path finds a config it cannot check and rejects the directory.
    "cmake/sen/sen-config-version.cmake",
    "lib/cmake/sen/sen_targets.cmake",
    "lib/cmake/sen/sen-config-version.cmake",
    "lib/cmake/sen/util/sen_utils.cmake",
    # configure_exportable_packages generates a consumer's forwarding config from this template, so
    # a package built against an archive without it fails at install time rather than at configure.
    "lib/cmake/sen/util/exportable-config-compat.cmake.in",
    "lib/cmake/sen/util/exportable-config-version-compat.cmake.in",
)

REQUIRED_DIRECTORIES = (
    "include/sen/core",
    # Where the vendored spdlog went when it left <prefix>/include. A consumer reaches it through
    # SEN_THIRD_PARTY_INCLUDE_DIR, and find_package_handle_standard_args checks only that the
    # variable is non-empty -- so a missing directory reports "found".
    "third_party/include/spdlog",
    # fmt installs on the line beside spdlog, unconditionally, and a consumer including
    # <spdlog/spdlog.h> pulls fmt headers through it -- so a package with one and not the other
    # compiles for us and fails for them.
    "third_party/include/fmt",
    "resources/syntax_highlighting",
)

# Named without prefix or extension because those differ by platform: libcore.so,
# libcore.dylib, core.dll. Nothing above covers a shared library, so an archive shipping none
# satisfied every entry.
#
# core is linked by the sen executable. shell is a component, and a missing component is the
# silent case: components are opened by name only when a config asks for one. shell ships in
# basic and full; a barebones package has none.
REQUIRED_LIBRARIES = ("core", "shell")

# Third-party shared objects install(RUNTIME_DEPENDENCY_SET) is expected to have resolved from the
# binaries that link them. That rule is the one new shipping mechanism in this layout and its silent
# failure is "resolved nothing" -- an archive missing every one of them satisfies all the entries
# above, which was demonstrated by deleting them from a real package.
#
# POSIX only, because the dependency set is not enrolled on Windows: a Windows archive correctly
# ships no vendored SDL, so requiring it everywhere would fail a good package. Matched on the SONAME
# prefix rather than a full filename so an SDL patch release does not fail the check.
POSIX_REQUIRED_LIBRARY_PREFIXES = ("libSDL2",)

# sen-<version>-<processor>-<system>-<compiler>-<version>-<build type>, lower
# case. The version is a tag or "latest", and a tag may carry an -rc suffix.
NAME_PATTERN = re.compile(r"^sen-[^-]+(?:-rc\d+)?-[^-]+-(?P<system>[^-]+)-[^-]+-[^-]+-(?:release|debug)$")


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


def archive_stem(archive: Path) -> str:
    """Returns the archive name without its extension."""
    name = archive.name
    for suffix in (".tar.gz", ".zip"):
        if name.endswith(suffix):
            return name[: -len(suffix)]

    raise SystemExit(f"Error: unsupported archive type: {name}")


def missing_entries(entries: list[str], system: str | None = None) -> list[str]:
    """Returns the required files and directories the archive does not hold.

    `system` is the platform read out of the archive name. None means it could not be read,
    in which case the platform-specific requirements are not applied rather than guessed.
    """
    present = set(entries)
    missing = [name for name in REQUIRED_FILES if name not in present and f"{name}.exe" not in present]
    missing += [name for name in REQUIRED_DIRECTORIES if not any(entry.startswith(f"{name}/") for entry in entries)]
    missing += [
        f"{name} (shared library)"
        for name in REQUIRED_LIBRARIES
        if not any(_is_shared_library(entry, name) for entry in entries)
    ]
    if system is not None and system != "windows":
        missing += [
            f"lib/{prefix}* (shared library)"
            for prefix in POSIX_REQUIRED_LIBRARY_PREFIXES
            if not any(entry.startswith(f"lib/{prefix}") for entry in entries)
        ]
    return missing


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
    match = NAME_PATTERN.match(archive_stem(archive))
    if not match:
        problems.append(f"name does not match the expected pattern: {archive.name}")

    system = match.group("system") if match else None
    problems.extend(f"missing entry: {entry}" for entry in missing_entries(list_entries(archive), system))
    return problems


def main() -> int:
    """Checks the archives found in the given build directory."""
    parser = argparse.ArgumentParser(
        prog="check_package_archive",
        description="Checks that the archive built by CPack is complete.",
    )
    parser.add_argument("build_dir", help="Build directory holding the archive that CPack wrote.")
    args = parser.parse_args()

    archives = sorted(Path(args.build_dir).glob("sen-*.tar.gz")) + sorted(Path(args.build_dir).glob("sen-*.zip"))
    if not archives:
        raise SystemExit(f"Error: no archive found in {args.build_dir}")

    problems = [problem for archive in archives for problem in check_archive(archive)]
    for problem in problems:
        print(problem)

    if problems:
        return 1

    print(f"checked {', '.join(archive.name for archive in archives)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
