# === test_conan_lock.py ===============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Tests the conan.lock management script against a mocked conan."""

import subprocess
from pathlib import Path

import conan_lock


def fake_conan(recorded: list[list[str]], content: str):
    """Builds a subprocess.run stand-in that records commands and writes the lockfile output."""

    def run(command, cwd=None, check=True, **kwargs):  # noqa: ARG001 - mirrors subprocess.run
        parts = [str(part) for part in command]
        recorded.append(parts)
        for part in parts:
            if part.startswith("--lockfile-out="):
                Path(part.split("=", 1)[1]).write_text(content, encoding="utf-8")
        return subprocess.CompletedProcess(parts, 0)

    return run


def test_update_resolves_every_profile_in_order(monkeypatch, tmp_path):
    """Update runs one resolution per profile and chains the lockfile through them."""
    lockfile = tmp_path / "conan.lock"
    recorded: list[list[str]] = []
    monkeypatch.setattr(conan_lock, "LOCKFILE", lockfile)
    monkeypatch.setattr(conan_lock.subprocess, "run", fake_conan(recorded, "LOCKED\n"))

    assert conan_lock.update() == 0
    assert lockfile.read_text(encoding="utf-8") == "LOCKED\n"

    profiles = [part for command in recorded for part in command if part.startswith(".conan/profiles/")]
    assert profiles == [f".conan/profiles/{profile}" for profile in conan_lock.PROFILES]
    assert not any(part.startswith("--lockfile=") for part in recorded[0])
    assert all(f"--lockfile={lockfile}" in command for command in recorded[1:])


def test_check_passes_when_resolution_matches(monkeypatch, tmp_path):
    """Check succeeds when resolving inside the lockfile changes nothing."""
    lockfile = tmp_path / "conan.lock"
    lockfile.write_text("LOCKED\n", encoding="utf-8")
    recorded: list[list[str]] = []
    monkeypatch.setattr(conan_lock, "LOCKFILE", lockfile)
    monkeypatch.setattr(conan_lock.subprocess, "run", fake_conan(recorded, "LOCKED\n"))

    assert conan_lock.check() == 0
    assert f"--lockfile={lockfile}" in recorded[0]


def test_check_fails_when_the_conanfile_outgrows_the_lock(monkeypatch, tmp_path, capsys):
    """Check fails and points at update when resolution adds entries."""
    lockfile = tmp_path / "conan.lock"
    lockfile.write_text("LOCKED\n", encoding="utf-8")
    monkeypatch.setattr(conan_lock, "LOCKFILE", lockfile)
    monkeypatch.setattr(conan_lock.subprocess, "run", fake_conan([], "LOCKED\nnew-dependency\n"))

    assert conan_lock.check() == 1
    captured = capsys.readouterr()
    assert "does not cover" in captured.err
    assert "new-dependency" in captured.out
