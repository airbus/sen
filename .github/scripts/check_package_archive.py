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
    "cmake/sen/sen_targets.cmake",
    "cmake/sen/SenConfigVersion.cmake",
    "cmake/sen/util/sen_utils.cmake",
)

REQUIRED_DIRECTORIES = (
    "libs/core/include/sen/core",
    "resources/syntax_highlighting",
)

# sen-<version>-<processor>-<system>-<compiler>-<version>-<build type>, lower
# case. The version is a tag or "latest", and a tag may carry an -rc suffix.
NAME_PATTERN = re.compile(r"^sen-[^-]+(?:-rc\d+)?-[^-]+-[^-]+-[^-]+-[^-]+-(release|debug)$")


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


def missing_entries(entries: list[str]) -> list[str]:
    """Returns the required files and directories the archive does not hold."""
    present = set(entries)
    missing = [name for name in REQUIRED_FILES if name not in present and f"{name}.exe" not in present]
    missing += [name for name in REQUIRED_DIRECTORIES if not any(entry.startswith(f"{name}/") for entry in entries)]
    return missing


def check_archive(archive: Path) -> list[str]:
    """Returns the problems found in the archive, empty when it is complete."""
    problems = []
    if not NAME_PATTERN.match(archive_stem(archive)):
        problems.append(f"name does not match the expected pattern: {archive.name}")

    problems.extend(f"missing entry: {entry}" for entry in missing_entries(list_entries(archive)))
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
