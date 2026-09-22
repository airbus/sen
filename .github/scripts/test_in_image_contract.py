"""Checks the environment in_image.sh gives the container it starts.

Three lanes set CC and CXX at job level and build through this script, so the
compiler they select reaches the compiler that runs only if the script forwards
it. Dropping the forwarding does not fail anything: the build succeeds with the
image's default compiler and the lane stays green while measuring something else.
"""

import os
import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / ".github" / "scripts" / "in_image.sh"
WORKFLOWS = ROOT / ".github" / "workflows"
TEXT = SCRIPT.read_text(encoding="utf-8")


def callers() -> list[Path]:
    """Workflows that set a compiler and build through the script."""
    found = []
    for path in sorted(WORKFLOWS.glob("*.yaml")):
        text = path.read_text(encoding="utf-8")
        if "in_image.sh" in text and re.search(r"^\s*CC:\s*\S", text, re.MULTILINE):
            found.append(path)
    return found


def test_a_lane_selects_its_compiler_this_way():
    """Fails when no lane does, which would leave the forwarding below guarding nothing."""
    assert callers(), "no workflow sets CC and builds through in_image.sh"


@pytest.mark.parametrize("name", ["CC", "CXX"])
def test_the_compiler_is_forwarded_from_the_caller(name):
    """The valueless form passes the caller's value through; --env CC=gcc would pin it."""
    assert re.search(rf"--env\s+{name}\b(?!=)", TEXT), f"{name} is not forwarded to the container"


@pytest.mark.parametrize("name", ["SEN_CI_IMAGE", "GITHUB_WORKSPACE"])
def test_a_missing_input_stops_the_run(name):
    """An unset input would otherwise start a container against the wrong image or path."""
    assert re.search(rf'^: "\$\{{{name}:\?', TEXT, re.MULTILINE), f"{name} may be unset"


def test_the_workspace_keeps_its_path():
    """Conan writes absolute paths, so a build folder belongs to the path that configured it."""
    assert '--volume "$GITHUB_WORKSPACE:$GITHUB_WORKSPACE"' in TEXT
    assert '--workdir "$GITHUB_WORKSPACE"' in TEXT


def test_home_is_writable():
    """There is no passwd entry for the caller inside, so conan gets / and cannot write."""
    assert re.search(r"--env\s+HOME=", TEXT), "HOME is left as the image set it"


# The variable alone points at an empty directory inside the container, and the mount
# alone leaves the image's own path in use. Either half on its own builds correctly and
# from a cold cache, so a lane that restored one keeps paying for every compile.
CACHES = [("CONAN_HOME", "/conan", "$HOME/.conan2"), ("CCACHE_DIR", "/ccache", "$HOME/.ccache")]


@pytest.mark.parametrize(("name", "inside", "outside"), CACHES, ids=[c[0] for c in CACHES])
def test_a_cache_is_redirected_and_mounted(name, inside, outside):
    """Both halves, because either alone builds correctly from a permanently cold cache."""
    assert re.search(rf"--env\s+{name}={re.escape(inside)}\b", TEXT), f"{name} is left as the image set it"
    assert f'--volume "{outside}:{inside}"' in TEXT, f"nothing is mounted at {inside}"


def test_the_build_output_belongs_to_the_caller():
    """Root-owned output fails whoever reads it afterwards, not the build that wrote it."""
    assert '--user "$(id -u):$(id -g)"' in TEXT, "the container runs as the image's user"


def test_the_sanitizers_can_run():
    """Some kernels refuse the calls a sanitized binary makes under the default profile."""
    assert "--security-opt seccomp=unconfined" in TEXT, "the seccomp profile is left at the default"


# Everything after the literal "docker run", which is what the container is given
# unconditionally. A mount added here reaches every caller.
RUN_ARGUMENTS = TEXT.split("docker run", 1)[1]


def test_the_daemon_socket_is_not_handed_out_by_default():
    """The socket carries the daemon's full authority, and one lane's suites need it."""
    assert "docker.sock" not in RUN_ARGUMENTS


def test_the_daemon_socket_can_be_asked_for():
    """And the asking is by name, so a reader can find who turned it on."""
    guard = re.search(r"if \[ -n \"\$\{SEN_IN_IMAGE_DOCKER:-\}\" \][\s\S]{0,240}?\nfi\n", TEXT)
    assert guard, "SEN_IN_IMAGE_DOCKER guards nothing"
    assert "docker.sock" in guard.group(0)


def test_the_script_arrives_on_standard_input():
    """An argument would need the caller to quote the script it sends."""
    assert "--interactive" in TEXT
    assert re.search(r"bash -s", TEXT), "the container does not read the script from stdin"


# The assertions above read the file. They cannot see a shell that refuses to run it:
# an empty array expanded under `set -u` is a syntax the runners' bash 5 accepts and the
# bash 3.2 macOS ships does not, so the default path failed for a developer while every
# text assertion passed. These run the script under whatever bash is here, against a
# stub daemon, and read the arguments it would have given docker.
def run_the_script(tmp_path, **environment) -> list[str]:
    """Runs in_image.sh with docker stubbed, returning the arguments it passed."""
    recorded = tmp_path / "arguments"
    stub = tmp_path / "docker"
    stub.write_text(f'#!/bin/sh\nfor a in "$@"; do echo "$a" >> {recorded}; done\n')
    stub.chmod(0o755)
    finished = subprocess.run(
        ["bash", str(SCRIPT)],
        input="",
        capture_output=True,
        text=True,
        # Checked below, so the assertion can show what the shell said.
        check=False,
        env={
            "PATH": f"{tmp_path}:{os.environ['PATH']}",
            "HOME": str(tmp_path),
            "SEN_CI_IMAGE": "sen-ci:test",
            "GITHUB_WORKSPACE": str(tmp_path),
            **environment,
        },
    )
    assert finished.returncode == 0, finished.stderr
    return recorded.read_text().splitlines()


@pytest.mark.skipif(shutil.which("bash") is None, reason="needs a shell to run the script")
def test_the_script_runs_under_this_shell(tmp_path):
    """Whatever bash is here, not the one in the image or on a runner."""
    assert "run" in run_the_script(tmp_path)


@pytest.mark.skipif(shutil.which("bash") is None, reason="needs a shell to run the script")
def test_the_run_it_builds_leaves_the_socket_out(tmp_path):
    """The default path, as a caller would get it rather than as the file reads."""
    arguments = run_the_script(tmp_path)
    assert not any("docker.sock" in argument for argument in arguments)
    assert "CC" in arguments


@pytest.mark.skipif(shutil.which("bash") is None, reason="needs a shell to run the script")
def test_the_run_it_builds_carries_the_socket_when_asked(tmp_path):
    """And the asked-for path actually reaches docker with the mount."""
    arguments = run_the_script(tmp_path, SEN_IN_IMAGE_DOCKER="1")
    assert "/var/run/docker.sock:/var/run/docker.sock" in arguments
