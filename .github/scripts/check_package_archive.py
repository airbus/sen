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
import os
import re
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path

# What the archive must hold, as paths under its top-level directory. Keep the
# lists short: one representative of each thing an install rule ships. Files and
# directories are separate, so a directory cannot stand in for a missing file.
REQUIRED_FILES = (
    "share/doc/sen/LICENSE.txt",
    # sen.exe on Windows; either spelling satisfies this entry.
    "bin/sen",
    # A second executable, because bin/sen alone is satisfied by an archive that dropped every
    # other install(TARGETS ... RUNTIME) rule.
    "bin/cli_gen",
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
    "share/sen/resources/syntax_highlighting",
    # The interfaces a consumer imports. libs/db installs here unconditionally, so this holds for
    # every package including a barebones one.
    "share/sen/interfaces",
    # Component-installed, and present wherever libshell is: ether and shell both install a schema
    # and both are in `basic`, so this entry holds in exactly the builds that one does.
    "share/sen/schemas",
    # One representative per install rule this change edited, so a dropped rule fails rather than
    # being covered by a neighbour: the generated interface headers, the licence tree, and a second
    # header root are each installed by their own rule.
    "include/stl/sen",
    "include/sen/kernel",
    # Harvested by the conan recipe's generate(), so a release archive always has it and a plain
    # cmake build has none. Required, because the input to this check is a release archive.
    "share/doc/sen/foss_licenses",
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

# The Python extension module. Its directory differs by platform for a reason: python 3.8+ resolves
# an extension's dependencies from the module's own directory and never from PATH, so on Windows it
# must sit with the executables. Matched on the stem, since the suffix varies.
PYTHON_MODULE_STEM = "sen_db_python"

# The upper bound: install.cmake excludes by directory, which bounds nothing when the toolchain
# lives somewhere unusual -- a pyenv Python is copied in silently. Shipping one of these is wrong
# even when it works, since $ORIGIN/../lib is searched first. The prefixes carry punctuation so they
# cannot match Sen's own: "libc." rather than "libc", or libcore would match.
FORBIDDEN_LIBRARY_PREFIXES = (
    "libc.",
    "libc-",
    "libstdc++",
    "libgcc_s",
    "libm.",
    "libm-",
    "libpthread",
    "libdl.",
    "librt.",
    "libpython",
    "libGL",
    "libEGL",
    "libOpenGL",
    "libGLESv2",
    "libX11",
    "libXext",
    "libvulkan",
    # The mach-o spellings, without which the list is a no-op on a macOS archive: libc++ is not
    # libstdc++, libc is libSystem, and an embedded Python is a framework binary named Python.
    "libc++.",
    "libSystem.",
    "libobjc.",
    "Python",
)

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
    # Conditional on libpy: both are behind SEN_BUILD_PY, so an archive built without it correctly
    # has neither.
    if system is not None and any(_is_shared_library(entry, "py") for entry in entries):
        module_directory = "bin" if system == "windows" else "lib"
        if not any(
            entry.rpartition("/")[0] == module_directory and entry.rpartition("/")[2].startswith(PYTHON_MODULE_STEM)
            for entry in entries
        ):
            missing.append(f"{module_directory}/{PYTHON_MODULE_STEM}* (python extension module)")

    # Conditional on the component that pulls it in: the vendored SDL arrives with the explorer, so
    # an archive built without it correctly has neither.
    if system is not None and system != "windows" and any(_is_shared_library(e, "explorer") for e in entries):
        missing += [
            f"lib/{prefix}* (shared library)"
            for prefix in POSIX_REQUIRED_LIBRARY_PREFIXES
            if not any(entry.startswith(f"lib/{prefix}") for entry in entries)
        ]
    return missing


def forbidden_entries(entries: list[str]) -> list[str]:
    """Returns the shipped libraries that the target system is supposed to provide."""
    found = []
    for entry in entries:
        directory, _, stem = entry.rpartition("/")
        if directory not in ("lib", "bin"):
            continue
        if stem.startswith(FORBIDDEN_LIBRARY_PREFIXES):
            found.append(entry)
    return found


def _is_shared_library(entry: str, name: str) -> bool:
    """Whether an archive entry is the shared library `name`, where the loader looks for it.

    The directory follows from the spelling rather than the platform: a DLL is a runtime
    artefact and ships beside the executables, while .so and .dylib ship in the library
    directory. A versioned suffix counts on both, since libcore.so is a symlink to
    libcore.so.0.0.0 and the mach-o spelling of the same thing is libcore.0.0.0.dylib -- the
    trailing dot in the prefix is what keeps libcore from matching libcoreutils.
    """
    directory, _, stem = entry.rpartition("/")
    if stem == f"{name}.dll":
        return directory == "bin"

    if stem.startswith(f"lib{name}.") and (stem.endswith(".dylib") or ".so" in stem):
        return directory == "lib"

    return False


def check_archive(archive: Path) -> list[str]:
    """Returns the problems found in the archive, empty when it is complete."""
    problems = []
    match = NAME_PATTERN.match(archive_stem(archive))
    if not match:
        problems.append(f"name does not match the expected pattern: {archive.name}")

    entries = list_entries(archive)
    system = match.group("system") if match else None
    problems.extend(f"missing entry: {entry}" for entry in missing_entries(entries, system))
    problems.extend(
        f"the target system provides this, it must not ship: {entry}" for entry in forbidden_entries(entries)
    )
    return problems


def _extract(archive: Path, destination: Path) -> None:
    """Unpacks the archive, refusing any member that would land outside `destination`."""
    if archive.suffixes[-2:] == [".tar", ".gz"]:
        with tarfile.open(archive) as tar:
            tar.extractall(destination, filter="data")
    else:
        with zipfile.ZipFile(archive) as zip_file:
            zip_file.extractall(destination)


def unpack_and_run(archive: Path) -> list[str]:
    """Unpacks the archive somewhere new and starts the executable it ships.

    Everything above this reads the list of names in the archive, which cannot see the failure
    this layout change is most likely to produce: the files are all present and correct, and the
    loader cannot find them anyway because a run path is wrong. That archive passes every entry
    above and dies with "error while loading shared libraries" in the user's hands.

    Unpacked to a fresh directory, and run with the library-path variables removed from the
    environment, so what is measured is the archive resolving its own contents rather than the
    build machine's.
    """
    with tempfile.TemporaryDirectory() as tmp:
        destination = Path(tmp)
        # filter="data" refuses members that escape the destination. This is a command line tool
        # someone points at a downloaded release, and --execute then runs a binary out of it.
        try:
            _extract(archive, destination)
        except (OSError, EOFError, tarfile.TarError, zipfile.BadZipFile) as error:
            # Reported rather than raised, so a truncated download or a member that escapes the
            # destination reads as a verdict on the archive instead of a traceback from the tool.
            return [f"the archive could not be unpacked: {type(error).__name__}: {error}"]

        roots = [entry for entry in destination.iterdir() if entry.is_dir()]
        if len(roots) != 1:
            return [f"expected one top-level directory when unpacked: {sorted(e.name for e in roots)}"]

        executable = roots[0] / "bin" / ("sen.exe" if os.name == "nt" else "sen")
        if not executable.exists():
            return [f"no executable to run at bin/{executable.name}"]
        executable.chmod(0o755)

        environment = {k: v for k, v in os.environ.items() if k not in ("LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH")}
        try:
            result = subprocess.run(  # noqa: S603
                [str(executable), "--version"],
                capture_output=True,
                text=True,
                timeout=60,
                env=environment,
                check=False,  # a non-zero exit is the finding, not an error to raise
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            return [f"the shipped executable did not run: {error}"]

        if result.returncode != 0:
            detail = (result.stderr or result.stdout).strip().splitlines()
            return [f"the shipped executable did not run: {detail[-1] if detail else result.returncode}"]
    return []


def main() -> int:
    """Checks the archives found in the given build directory."""
    parser = argparse.ArgumentParser(
        prog="check_package_archive",
        description="Checks that the archive built by CPack is complete.",
    )
    parser.add_argument("build_dir", help="Build directory holding the archive that CPack wrote.")
    parser.add_argument(
        "--execute",
        action="store_true",
        help="Also unpack each archive elsewhere and run the executable it ships, which is the only "
        "way to catch a complete archive whose run paths do not resolve.",
    )
    args = parser.parse_args()

    archives = sorted(Path(args.build_dir).glob("sen-*.tar.gz")) + sorted(Path(args.build_dir).glob("sen-*.zip"))
    if not archives:
        raise SystemExit(f"Error: no archive found in {args.build_dir}")

    problems = [problem for archive in archives for problem in check_archive(archive)]
    if args.execute:
        problems += [problem for archive in archives for problem in unpack_and_run(archive)]
    for problem in problems:
        print(problem)

    if problems:
        return 1

    print(f"checked {', '.join(archive.name for archive in archives)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
