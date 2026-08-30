# === lanes.py =========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""The CI lanes, defined once and runnable without pushing.

A lane is a compiler, a build type and a set of conan options, followed by one cmake
target. That is the whole shape, and every lane in the workflows fits it.

The definitions here are the ones the workflows run: test_lanes.py compares each lane
against the workflow that carries it, so a lane that drifts from CI fails a test rather
than reporting a verdict that CI would not give.

A lane runs inside the CI image, built from tools/ci/Dockerfile, as the invoking user.
That is what makes a local answer comparable to CI's. It is not the same answer: the
runners are x86 and a developer machine may not be.
"""

import argparse
import dataclasses
import os
import platform
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
IMAGE = "sen-ci:base"

# Kept out of the checkout and out of the devcontainer's volumes. Those are owned by the
# image's user, and a container running as the invoking uid cannot write to them.
STATE = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache")) / "sen-lanes"


@dataclasses.dataclass(frozen=True, kw_only=True)
class Lane:
    """One CI lane, and where it runs today."""

    name: str
    summary: str
    # The workflow and job that carry this lane now. test_lanes.py reads them.
    workflow: str
    job: str
    compiler: str
    compiler_version: int
    build_type: str
    cppstd: int | None = None
    # Conan options, without the -o that introduces each of them.
    options: tuple[str, ...] = ()
    # cmake cache variables passed through conan, in the order the workflow writes them.
    extra_variables: tuple[tuple[str, str], ...] = ()
    # The target built after conan. A lane whose whole work is the compile has none.
    target: str | None = None
    # A script under .github/scripts run afterwards, whatever the build did.
    report: tuple[str, ...] = ()

    @property
    def build_dir(self) -> str:
        """Where conan puts this lane's build, from build_folder_vars in the profiles."""
        return f"build/{self.compiler}/{self.build_type}"

    @property
    def cc(self) -> str:
        """The C compiler the workflow sets for this lane."""
        return (
            f"{self.compiler}-{self.compiler_version}" if self.compiler == "clang" else f"gcc-{self.compiler_version}"
        )

    @property
    def cxx(self) -> str:
        """The C++ compiler the workflow sets for this lane."""
        return f"clang++-{self.compiler_version}" if self.compiler == "clang" else f"g++-{self.compiler_version}"


# The standard test matrix is absent: its legs are data in generate_matrix_jobs.py
# already, and its steps carry matrix expressions this would have to evaluate.
LANES = (
    Lane(
        name="asan-ubsan",
        summary="the address and undefined-behaviour gate that every pull request runs",
        workflow="pr_sanitizers.yaml",
        job="sanitizers",
        compiler="clang",
        compiler_version=20,
        build_type="Debug",
        options=("sen/*:with_tests=True", "sen/*:sanitizer=address"),
        extra_variables=(("SEN_SANITIZER_LOG_DIR", "$PWD/sanitizer-reports"), ("SEN_SANITIZER_FAIL_FAST", "ON")),
        target="run_tests",
        report=("summarise_sanitizer_reports.py", "sanitizer-reports"),
    ),
    Lane(
        name="tsan",
        summary="the nightly thread-sanitizer lane, which collects rather than gates",
        workflow="nightly.yaml",
        job="sanitizers",
        compiler="clang",
        compiler_version=20,
        build_type="Debug",
        options=("sen/*:with_tests=True", "sen/*:sanitizer=thread"),
        extra_variables=(("SEN_SANITIZER_LOG_DIR", "$PWD/sanitizer-reports"),),
        target="run_tests",
        report=("summarise_sanitizer_reports.py", "sanitizer-reports"),
    ),
    Lane(
        name="clang-tidy",
        summary="the nightly analysis of the targets that opted in",
        workflow="nightly.yaml",
        job="clang-tidy",
        compiler="clang",
        compiler_version=20,
        build_type="Release",
        options=("sen/*:with_tests=True", "sen/*:with_clang_tidy=True"),
    ),
    Lane(
        name="coverage",
        summary="the nightly coverage report, which runs the suite as it measures it",
        workflow="nightly.yaml",
        job="coverage",
        compiler="clang",
        compiler_version=20,
        build_type="Debug",
        cppstd=17,
        options=("sen/*:with_tests=True", "sen/*:with_coverage=True"),
        target="generate-coverage-report",
    ),
)

BY_NAME = {lane.name: lane for lane in LANES}


def _settings(lane: Lane) -> list[str]:
    """The settings both conan commands take, applied to the sen package alone."""
    settings = ["-s", f"&:build_type={lane.build_type}"]
    if lane.cppstd is not None:
        settings += ["-s", f"&:compiler.cppstd={lane.cppstd}"]
    return settings


def _options_and_conf(lane: Lane) -> list[str]:
    """The options and cmake variables both conan commands take."""
    arguments = [argument for option in lane.options for argument in ("-o", option)]
    if lane.extra_variables:
        body = ", ".join(f"'{name}': '{value}'" for name, value in lane.extra_variables)
        arguments += ["-c", f"tools.cmake.cmaketoolchain:extra_variables={{{body}}}"]
    return arguments


def commands(lane: Lane) -> list[list[str]]:
    """Everything the lane runs, in order, as argument lists.

    conan build takes the same options as conan install because it regenerates the
    toolchain; without them it would do so from the defaults, which build no tests.
    The argument order is the workflows' own, so test_lanes.py can compare literally.
    """
    settings, rest = _settings(lane), _options_and_conf(lane)
    result = [
        ["conan", "install", ".", *settings, "--build=missing", *rest],
        ["conan", "build", ".", *settings, *rest],
    ]
    if lane.target is not None:
        result.append(["source", f"{lane.build_dir}/generators/conanbuild.sh"])
        result.append(["cmake", "--build", f"{lane.build_dir}/", "--target", lane.target])
    if lane.report:
        script, *report_arguments = lane.report
        result.append(["python3", f".github/scripts/{script}", *report_arguments])
    return result


def profile_name(lane: Lane) -> str:
    """The repository profile this lane installs as the conan default.

    There is a per-architecture profile for gcc only, and the architecture-free ones
    detect it themselves, so clang falls back to the base profile.
    """
    architecture = "arm" if platform.machine() in ("aarch64", "arm64") else "x86"
    specific = ROOT / ".conan" / "profiles" / f"sen_{lane.compiler}_{architecture}"
    return specific.name if specific.exists() else f"sen_{lane.compiler}"


def setup_commands(lane: Lane) -> list[str]:
    """Installs the repository profiles and selects this lane's, as CI does.

    setup_build_context does this on a runner and the devcontainer's postCreateCommand
    does it again, so this is a third copy of it.
    """
    commands = [
        # The image gives a foreign uid no home of its own, and conan wants one.
        'mkdir -p "$HOME"',
        "conan profile detect --force -vquiet",
        'conan config install .conan/profiles/ --target-folder "$CONAN_HOME"/profiles/',
        f'cp .conan/profiles/{profile_name(lane)} "$CONAN_HOME"/profiles/default',
    ]
    # A runner reports on a fresh checkout; here the directory survives between runs,
    # and the summariser reads whatever is in it.
    for name, value in lane.extra_variables:
        if name.endswith("_LOG_DIR"):
            prefix, _, tail = value.partition("/")
            if prefix != "$PWD" or not tail.replace("-", "").replace("_", "").isalnum():
                raise ValueError(f"cannot clear a report directory spelled {value!r}")
            commands.append(f'rm -rf "$PWD"/{tail}')
    return commands


def script_for(lane: Lane) -> str:
    """The whole lane as one shell script, which is how the commands share an environment."""
    rendered = [" ".join(_quote(part) for part in command) for command in commands(lane)]
    lines = ["set -e", *setup_commands(lane)]
    if lane.report:
        # The workflow reports with `if: always()`, because a lane that failed is when
        # the findings are worth reading. The verdict stays the build's either way:
        # the summariser reports and never gates.
        *rendered, report = rendered
        lines += [f"report() {{ {report}; }}", "trap report EXIT"]
    return "\n".join([*lines, *rendered])


def _quote(part: str) -> str:
    """Quotes one argument, leaving a shell expansion inside it able to expand.

    extra_variables carries $PWD, which the workflows let the shell resolve, so that
    argument takes double quotes instead of going through shlex.
    """
    if "$" not in part:
        return shlex.quote(part)
    if any(character in part for character in '"\\`'):
        raise ValueError(f"cannot quote an argument that expands and also quotes: {part}")
    return f'"{part}"'


def _docker_arguments(lane: Lane) -> list[str]:
    """Runs the image as the invoking user, with the variables it cannot inherit.

    The image bakes CCACHE_DIR into another user's home, where a foreign uid cannot
    write, and ccache then degrades to a silent total miss rather than an error.
    """
    build_state = STATE / "build" / lane.name
    for directory in (STATE / "conan", STATE / "ccache", build_state):
        directory.mkdir(parents=True, exist_ok=True)
    mounts = {
        ROOT: "/workspace",
        # Over the checkout's own build folder. A build configured in a container
        # cannot be built on the host, so this leaves the developer's one alone.
        build_state: "/workspace/build",
        STATE / "conan": "/conan",
        STATE / "ccache": "/ccache",
    }
    environment = {"HOME": "/tmp/sen", "CONAN_HOME": "/conan", "CCACHE_DIR": "/ccache"}
    environment |= {"CC": lane.cc, "CXX": lane.cxx}
    if lane.compiler == "gcc":
        # The gcc profile reads this; without it the version reaches nothing.
        environment["SEN_GCC_VERSION"] = str(lane.compiler_version)

    arguments = ["docker", "run", "--rm", "-i", "--user", f"{os.getuid()}:{os.getgid()}"]
    for source, target in mounts.items():
        arguments += ["-v", f"{source}:{target}"]
    for name, value in environment.items():
        arguments += ["-e", f"{name}={value}"]
    return [*arguments, "-w", "/workspace", IMAGE, "bash", "-l", "-s"]


def build_image() -> None:
    """Builds the CI image from the Dockerfile that defines it."""
    print(f"building {IMAGE} from tools/ci/Dockerfile", file=sys.stderr)
    subprocess.run(
        ["docker", "build", "--target", "base", "-t", IMAGE, "-f", "tools/ci/Dockerfile", "tools/ci"],
        cwd=ROOT,
        check=True,
    )


def run_in_container(lane: Lane) -> int:
    """Runs the lane inside the CI image, leaving the checkout as it found it.

    conan install rewrites CMakeUserPresets.json beside the conanfile, and its paths
    belong to whichever environment configured the build, so a lane would replace the
    entries a developer's editor reads with ones pointing into the container.
    """
    build_image()
    presets = ROOT / "CMakeUserPresets.json"
    saved = presets.read_bytes() if presets.exists() else None
    try:
        script = script_for(lane)
        return subprocess.run(_docker_arguments(lane), input=script, text=True, cwd=ROOT, check=False).returncode
    finally:
        if saved is None:
            presets.unlink(missing_ok=True)
        else:
            presets.write_bytes(saved)


def main() -> int:
    """Runs one lane, or says which there are."""
    parser = argparse.ArgumentParser(prog="lanes", description=__doc__.splitlines()[0])
    parser.add_argument("lane", nargs="?", help="the lane to run")
    parser.add_argument("--list", action="store_true", help="list the lanes and stop")
    parser.add_argument("--print", action="store_true", help="print what the lane would run and stop")
    arguments = parser.parse_args()

    if arguments.list or arguments.lane is None:
        for lane in LANES:
            print(f"  {lane.name:<12} {lane.summary}")
            print(f"  {'':<12} runs today as {lane.job} in .github/workflows/{lane.workflow}")
        return 0 if arguments.list else 2

    if arguments.lane not in BY_NAME:
        parser.error(f"unknown lane {arguments.lane!r}; --list shows them")
    lane = BY_NAME[arguments.lane]

    if arguments.print:
        print(script_for(lane))
        return 0

    # Listing and printing work anywhere; running needs the Linux image, and the uid
    # handling below is POSIX. Windows is built by its own workflow leg.
    if os.name != "posix":
        print("a lane runs in the Linux CI image, which this platform cannot host", file=sys.stderr)
        return 1

    if os.getuid() == 0:
        # Artefacts would belong to root, which is what breaks the host steps that
        # follow a container job and what leaves a checkout nobody else can write.
        print("refusing to run as root: the build would own its output as root", file=sys.stderr)
        return 1
    return run_in_container(lane)


if __name__ == "__main__":
    raise SystemExit(main())
