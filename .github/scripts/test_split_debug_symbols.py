# === test_split_debug_symbols.py ======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins what the debug-symbol split decides before it touches anything.

The subject lives in cmake/util because the conan recipe exports that directory; the test
lives here because this is where the repository runs tests from.

The ELF fixtures are built byte by byte, not copied from a build, so the test states
what the parser is being asked to read and can be run anywhere.
"""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "cmake" / "util"))

from split_debug_symbols import build_id_of, collect_pdbs, main, plan  # noqa: E402

SHDR_SIZE = 64
NOTE_NAME = b"GNU\0"


def write_elf(path: Path, build_id: bytes | None) -> Path:
    """Writes a 64-bit little-endian ELF carrying at most a GNU build-id note."""
    names = b"\0.note.gnu.build-id\0.shstrtab\0"
    note = b""
    if build_id is not None:
        note = struct.pack("<III", len(NOTE_NAME), len(build_id), 3) + NOTE_NAME + build_id

    header = bytearray(64)
    header[0:4] = b"\x7fELF"
    header[4] = 2  # 64-bit
    header[5] = 1  # little-endian

    note_off = 64
    names_off = note_off + len(note)
    shoff = names_off + len(names)
    count = 3 if note else 2

    struct.pack_into("<Q", header, 0x28, shoff)
    struct.pack_into("<HHH", header, 0x3A, SHDR_SIZE, count, count - 1)

    def shdr(name_off: int, offset: int, size: int) -> bytes:
        out = bytearray(SHDR_SIZE)
        struct.pack_into("<I", out, 0, name_off)
        struct.pack_into("<Q", out, 0x18, offset)
        struct.pack_into("<Q", out, 0x20, size)
        return bytes(out)

    table = shdr(0, 0, 0)
    if note:
        table += shdr(1, note_off, len(note))
    table += shdr(20, names_off, len(names))

    path.write_bytes(bytes(header) + note + names + table)
    return path


def test_the_build_id_is_read_back(tmp_path):
    """The fixture states the id, so a parser that invents one is caught."""
    planted = bytes.fromhex("5158f1b0c6173b8f111723b3249990f8bd51ddc6")
    assert build_id_of(write_elf(tmp_path / "libsen.so", planted)) == planted.hex()


def test_an_elf_without_a_build_id_is_skipped(tmp_path):
    """A binary linked without --build-id has nothing to key symbols by."""
    assert build_id_of(write_elf(tmp_path / "plain.so", None)) is None


def test_a_file_that_is_not_an_elf_is_skipped(tmp_path):
    """The install tree holds headers, cmake files and licences as well as binaries."""
    text = tmp_path / "sen_targets.cmake"
    text.write_text("# not an elf\n", encoding="utf-8")
    assert build_id_of(text) is None


def test_the_debug_file_is_named_by_build_id(tmp_path):
    """The layout gdb searches: first byte as the directory, the rest as the file."""
    planted = bytes.fromhex("5158f1b0c6173b8f111723b3249990f8bd51ddc6")
    prefix, symbols = tmp_path / "install", tmp_path / "symbols"
    (prefix / "lib").mkdir(parents=True)
    write_elf(prefix / "lib" / "libsen.so", planted)

    splits = plan(prefix, symbols)
    assert [s.debug_file.relative_to(symbols).as_posix() for s in splits] == [
        "lib/debug/.build-id/51/58f1b0c6173b8f111723b3249990f8bd51ddc6.debug"
    ]


def test_symlinks_are_not_split(tmp_path):
    """libcore.so is a link to libcore.so.0.0.0; splitting both would strip one twice."""
    planted = bytes.fromhex("aa" * 20)
    prefix, symbols = tmp_path / "install", tmp_path / "symbols"
    (prefix / "lib").mkdir(parents=True)
    real = write_elf(prefix / "lib" / "libcore.so.0.0.0", planted)
    (prefix / "lib" / "libcore.so").symlink_to(real.name)

    assert [s.binary.name for s in plan(prefix, symbols)] == ["libcore.so.0.0.0"]


def test_a_tree_with_no_binaries_fails_loudly(tmp_path, capsys):
    """Silence here would mean the rule ran before anything was installed and said nothing."""
    empty = tmp_path / "install"
    empty.mkdir()
    assert main([str(empty), str(tmp_path / "symbols")]) == 1
    assert "nothing to separate" in capsys.readouterr().err


def test_program_databases_leave_the_tree_that_gets_packaged(tmp_path):
    """Windows needs no split, but the .pdb must not stay in the archive everyone downloads."""
    prefix, symbols = tmp_path / "install", tmp_path / "symbols"
    (prefix / "bin").mkdir(parents=True)
    (prefix / "bin" / "core.pdb").write_bytes(b"pdb")
    (prefix / "bin" / "core.dll").write_bytes(b"dll")

    assert [p.relative_to(symbols).as_posix() for p in collect_pdbs(prefix, symbols)] == ["bin/core.pdb"]
    assert not (prefix / "bin" / "core.pdb").exists()
    assert (prefix / "bin" / "core.dll").exists()


def test_the_scan_finds_binaries_anywhere_in_the_tree(tmp_path):
    """Bin and lib both, and the executables are not all in one place."""
    planted = bytes.fromhex("bb" * 20)
    prefix, symbols = tmp_path / "install", tmp_path / "symbols"
    for directory in ("bin", "lib"):
        (prefix / directory).mkdir(parents=True)
        write_elf(prefix / directory / f"thing_{directory}", planted)

    assert sorted(s.binary.name for s in plan(prefix, symbols)) == ["thing_bin", "thing_lib"]
