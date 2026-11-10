# === test_drop_cache_key.py ===========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Tests the drop_cache_key.sh script and its retry loop.

Since the script interacts with the GitHub CLI (gh), this uses a mocked executable
to simulate API responses, network failures, and eventual consistency delays.
"""

import os
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / ".github" / "scripts" / "drop_cache_key.sh"

MOCK_GH = """#!/usr/bin/env bash
# Mock for gh api
if [ "$2" = "-X" ] && [ "$3" = "DELETE" ]; then
    echo "Deleted $4" >> "${TMP_DIR}/deletes.log"
    exit 0
fi

# Track how many times list was called
count_file="${TMP_DIR}/list_calls"
count=$(cat "$count_file" 2>/dev/null || echo 0)
count=$((count + 1))
echo "$count" > "$count_file"

if [ -f "${TMP_DIR}/list_fail" ] && [ "$count" -gt 1 ]; then
    echo "API rate limit exceeded" >&2
    exit 1
fi

if [ -f "${TMP_DIR}/list_eventual" ]; then
    # Return full list on the first call (before delete) and second call (attempt 1)
    # Return empty list on the third call (attempt 2), simulating eventual consistency.
    if [ "$count" -le 2 ]; then
        cat "${TMP_DIR}/mock_caches.txt"
    else
        echo ""
    fi
    exit 0
fi

cat "${TMP_DIR}/mock_caches.txt"
"""


@pytest.fixture(autouse=True)
def mock_env(tmp_path: Path, monkeypatch: pytest.MonkeyPatch):
    """Sets up a fake environment with a mocked gh executable."""
    gh_mock = tmp_path / "gh"
    gh_mock.write_text(MOCK_GH, encoding="utf-8")
    gh_mock.chmod(0o755)

    (tmp_path / "mock_caches.txt").write_text("", encoding="utf-8")

    monkeypatch.setenv("PATH", f"{tmp_path}:{os.environ['PATH']}")
    monkeypatch.setenv("TMP_DIR", str(tmp_path))
    monkeypatch.setenv("GH_TOKEN", "fake_token")
    monkeypatch.setenv("GITHUB_REPOSITORY", "airbus/sen")
    monkeypatch.setenv("GITHUB_REF", "refs/heads/main")
    return tmp_path


def run_script(key: str) -> subprocess.CompletedProcess:
    """Executes the drop script."""
    return subprocess.run(["bash", str(SCRIPT), key], capture_output=True, text=True, check=False)


def test_bypasses_when_not_on_main(monkeypatch: pytest.MonkeyPatch):
    """The saves it accompanies run only on main, so it bails out on branches."""
    monkeypatch.setenv("GITHUB_REF", "refs/heads/feature")
    run = run_script("ccache-docs")
    assert run.returncode == 0
    assert "not on main" in run.stdout


def test_exits_successfully_if_nothing_matches(mock_env: Path):
    """If the cache list doesn't contain the key, it exits clean without looping."""
    (mock_env / "mock_caches.txt").write_text("123 unrelated-key\n", encoding="utf-8")
    run = run_script("ccache-docs")
    assert run.returncode == 0
    assert "nothing under 'ccache-docs'" in run.stdout


def test_fails_after_retries_if_list_read_fails(mock_env: Path):
    """A network failure triggers the retry loop, then fails if it never recovers."""
    # Add a mock cache entry so the script doesn't exit early
    (mock_env / "mock_caches.txt").write_text("123 ccache-docs\n", encoding="utf-8")
    (mock_env / "list_fail").touch()

    run = run_script("ccache-docs")

    assert run.returncode == 1
    # Check that it tried to retry twice before the final failure
    assert run.stdout.count("retrying in 3s") == 2
    assert "cannot re-list caches to confirm the deletes landed:" in run.stderr


def test_eventual_consistency_succeeds_on_second_attempt(mock_env: Path):
    """The cache is deleted, but the first read after deletion still returns it. It succeeds on the next."""
    (mock_env / "mock_caches.txt").write_text("456 ccache-docs-2023-01-01T\n", encoding="utf-8")
    (mock_env / "list_eventual").touch()

    run = run_script("ccache-docs")
    assert run.returncode == 0

    deletes = (mock_env / "deletes.log").read_text()
    assert "456" in deletes
    assert "retrying in 3s" in run.stdout


def test_fails_if_cache_survives_all_attempts(mock_env: Path):
    """If the cache never drops from the GET request, it fails the job."""
    # We do not touch list_eventual, so the mock always returns the cache as present.
    (mock_env / "mock_caches.txt").write_text("789 ccache-docs\n", encoding="utf-8")

    run = run_script("ccache-docs")
    assert run.returncode == 1
    assert "entries this run set out to delete are still present" in run.stderr
    assert "789" in run.stderr
