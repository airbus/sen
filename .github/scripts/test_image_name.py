"""Checks image_name.sh, whose output decides which image a lane runs.

The tag carries the Dockerfile's content. If that stopped happening, a lane would
pull the previous environment and report nothing, so the content test below is the
one that matters; the rest guard the shape callers depend on.
"""

import os
import subprocess
from pathlib import Path

import pytest

SCRIPT = Path(__file__).resolve().parent / "image_name.sh"


def _env():
    """Strips git's own variables from the environment.

    git exports GIT_DIR into hooks and tests inherit it, which would make the
    fixture repository below resolve to the real one.
    """
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    env["PATH"] = os.environ["PATH"]
    return env


def _run(cwd, *args, registry=None):
    env = _env()
    if registry is not None:
        env["SEN_CI_REGISTRY"] = registry
    return subprocess.run(
        ["bash", str(SCRIPT), *args],
        cwd=cwd,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )


@pytest.fixture
def repo(tmp_path):
    """Builds a repository shaped like this one.

    The script reads tools/ci/Dockerfile, so the fixture has to have one.
    """
    env = _env()
    subprocess.run(["git", "init", "-q", str(tmp_path)], env=env, check=True)
    dockerfile = tmp_path / "tools" / "ci" / "Dockerfile"
    dockerfile.parent.mkdir(parents=True)
    dockerfile.write_text("FROM ubuntu:22.04\n")
    return tmp_path


def test_a_local_build_gets_a_bare_tag(repo):
    """A developer without a registry gets a local tag."""
    out = _run(repo, "base").stdout.strip()
    assert out.startswith("sen-ci:base-"), out


def test_the_registry_prefixes_every_variant(repo):
    """Setting the registry moves every caller at once."""
    out = _run(repo, "dev", registry="ghcr.io/airbus/").stdout.strip()
    assert out.startswith("ghcr.io/airbus/sen-ci:dev-"), out


def test_a_registry_without_a_trailing_slash_is_refused(repo):
    """It would silently produce a wrong reference."""
    result = _run(repo, "dev", registry="ghcr.io/airbus")
    assert result.returncode == 2
    assert "must end in a slash" in result.stderr


def test_a_missing_variant_is_refused(repo):
    """There is no sensible default variant."""
    assert _run(repo).returncode != 0


def test_editing_the_dockerfile_moves_the_tag(repo):
    """The load-bearing one.

    Without this a consumer pulls the previous environment and reports nothing.
    """
    before = _run(repo, "base").stdout.strip()

    (repo / "tools" / "ci" / "Dockerfile").write_text("FROM ubuntu:22.04\nRUN true\n")
    after = _run(repo, "base").stdout.strip()
    assert before != after, f"the tag did not move: {before}"

    (repo / "tools" / "ci" / "Dockerfile").write_text("FROM ubuntu:22.04\n")
    assert _run(repo, "base").stdout.strip() == before, "the tag did not come back"


def test_variants_do_not_collide(repo):
    """The base and dev stages are different images."""
    assert _run(repo, "base").stdout.strip() != _run(repo, "dev").stdout.strip()


def test_the_layer_cache_reference_is_stable_across_content(repo):
    """It must not move with the Dockerfile.

    A build imports it to skip work the last content already did.
    """
    before = _run(repo, "buildcache").stdout.strip()
    assert before == "sen-ci:buildcache", before

    (repo / "tools" / "ci" / "Dockerfile").write_text("FROM ubuntu:22.04\nRUN true\n")
    assert _run(repo, "buildcache").stdout.strip() == before


def test_the_layer_cache_is_not_confused_with_an_image(repo):
    """One is content-tagged and the other is not."""
    assert _run(repo, "buildcache").stdout.strip() != _run(repo, "base").stdout.strip()
