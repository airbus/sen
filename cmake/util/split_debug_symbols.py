# === split_debug_symbols.py ===========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Moves debug information out of installed ELF binaries into separate files.

Run by the install rules when SEN_RELEASE_SYMBOLS is on. What ships keeps the same code and
loses the debug sections; the sections go to a file named after the binary's GNU build id,
which is how a debugger and a minidump reader find them again.

Lives under cmake/ because the conan recipe exports that directory and not tools/ or
.github/, so a build from source has to be able to reach it. Its test sits in
.github/scripts, which is where this repository runs tests from.
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

BUILD_ID_NOTE = ".note.gnu.build-id"


@dataclass(frozen=True)
class Split:
    """One binary and the debug file it will be separated into."""

    binary: Path
    build_id: str
    debug_file: Path


def _sections(data: bytes) -> list[tuple[str, int, int]]:
    """Returns (name, offset, size) for every section of a 64-bit little-endian ELF."""
    (e_shoff,) = struct.unpack_from("<Q", data, 0x28)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    (shstr_off,) = struct.unpack_from("<Q", data, e_shoff + e_shstrndx * e_shentsize + 0x18)
    out = []
    for index in range(e_shnum):
        base = e_shoff + index * e_shentsize
        (name_off,) = struct.unpack_from("<I", data, base)
        offset, size = struct.unpack_from("<QQ", data, base + 0x18)
        end = data.index(b"\0", shstr_off + name_off)
        out.append((data[shstr_off + name_off : end].decode(), offset, size))
    return out


def build_id_of(path: Path) -> str | None:
    """Returns the GNU build id as hex, or None if the file is not an ELF that carries one."""
    data = path.read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 2:
        return None
    for name, offset, _ in _sections(data):
        if name == BUILD_ID_NOTE:
            name_size, desc_size, _ = struct.unpack_from("<III", data, offset)
            desc = offset + 12 + ((name_size + 3) // 4) * 4
            return data[desc : desc + desc_size].hex()
    return None


def plan(prefix: Path, symbols_root: Path) -> list[Split]:
    """Decides what will be split, without touching anything.

    Separated from the doing so a test can check the decisions on a tree of fixtures
    without binutils present.
    """
    splits = []
    for path in sorted(prefix.rglob("*")):
        if not path.is_file() or path.is_symlink():
            continue
        build_id = build_id_of(path)
        if build_id is None:
            continue
        debug_file = symbols_root / "lib" / "debug" / ".build-id" / build_id[:2] / f"{build_id[2:]}.debug"
        splits.append(Split(binary=path, build_id=build_id, debug_file=debug_file))
    return splits


def apply(splits: list[Split], objcopy: str) -> None:
    """Performs each split: keep the debug sections, strip them, then link the two."""
    for split in splits:
        split.debug_file.parent.mkdir(parents=True, exist_ok=True)
        _run([objcopy, "--only-keep-debug", split.binary, split.debug_file])
        _run([objcopy, "--strip-debug", split.binary])
        # Run from the debug file's directory and name it bare, so the link records a name
        # and not this machine's layout. The binary is absolute because the directory moved.
        _run(
            [objcopy, f"--add-gnu-debuglink={split.debug_file.name}", split.binary],
            cwd=split.debug_file.parent,
        )


def _run(command: list, cwd: Path | None = None) -> None:
    """Runs objcopy, reporting what failed instead of raising through the install."""
    result = subprocess.run([str(part) for part in command], cwd=cwd, check=False)
    if result.returncode != 0:
        raise SystemExit(f"split_debug_symbols: {command[0]} failed on {command[-1]}")


def collect_pdbs(prefix: Path, symbols_root: Path) -> list[Path]:
    """Moves MSVC program databases out of the tree about to be packaged.

    Windows needs no splitting: the linker already put the debug information in a .pdb
    beside the binary. What it needs is for that file to leave the archive everyone
    downloads, which is the same thing the split achieves on Linux.
    """
    moved = []
    for pdb in sorted(prefix.rglob("*.pdb")):
        destination = symbols_root / pdb.relative_to(prefix)
        destination.parent.mkdir(parents=True, exist_ok=True)
        pdb.replace(destination)
        moved.append(destination)
    return moved


def main(argv: list[str] | None = None) -> int:
    """Separates debug information from the given prefix, and refuses to find nothing."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prefix", type=Path, help="the install tree to split in place")
    parser.add_argument("symbols", type=Path, help="where the debug files are written")
    parser.add_argument("--objcopy", default="objcopy")
    args = parser.parse_args(argv)

    # Absolute throughout: the debuglink step runs from another directory, and a relative
    # path to the binary stops resolving the moment it does.
    splits = plan(args.prefix.resolve(), args.symbols.resolve())
    apply(splits, args.objcopy)
    moved = collect_pdbs(args.prefix.resolve(), args.symbols.resolve())

    if not splits and not moved:
        # A tree with neither an ELF binary nor a .pdb means the rule ran somewhere it was not
        # meant to, or ran before anything was installed. Either way it must not report success.
        print(f"split_debug_symbols: nothing to separate under {args.prefix}", file=sys.stderr)
        return 1

    print(f"split_debug_symbols: {len(splits)} split, {len(moved)} moved -> {args.symbols}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
