# === record_environment.py ============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Records the machine and toolchain a test run happened on.

A test result is only reproducible against the environment that produced it, and an environment
typed into a command line is one nobody checks: this report named x86_64 for a run on an arm64
machine until the day this was written. So it is read from the machine instead, by the same run
that produced the results, and the document prints what it was handed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

MEBIBYTE = 1024
# A version banner can run to several lines of licence; the first is the one that names the tool.
TOOLS = (
    ("CMake", ("cmake", "--version")),
    ("Conan", ("conan", "--version")),
    ("Ninja", ("ninja", "--version")),
    ("Python", ("python3", "--version")),
    ("typst", ("typst", "--version")),
)


def first_line(command: tuple[str, ...]) -> str:
    """The first line a tool prints for its version, or nothing when it is not installed."""
    if shutil.which(command[0]) is None:
        return ""
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=20, check=False)
    except (OSError, subprocess.SubprocessError):
        return ""
    # MSVC's cl prints its banner on stderr and nothing on stdout, which left the Windows
    # compiler recorded as its own name with no version at all.
    for stream in (result.stdout, result.stderr):
        if stream.strip():
            return stream.strip().splitlines()[0].strip()
    return ""


def distribution() -> str:
    """The name the distribution gives itself."""
    release = Path("/etc/os-release")
    if not release.is_file():
        return platform.platform()
    for line in release.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("PRETTY_NAME="):
            return line.split("=", 1)[1].strip().strip('"')
    return platform.platform()


def processor() -> str:
    """What the kernel calls the processor.

    x86 kernels publish a model name in cpuinfo. Arm ones do not: there the file carries only
    implementer and part codes, so lscpu's vendor is the most that can be said truthfully. The
    answer names its own limit rather than decoding a part number into a marketing name.
    """
    info = Path("/proc/cpuinfo")
    if info.is_file():
        for line in info.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.lower().startswith(("model name", "hardware")):
                return line.split(":", 1)[1].strip()

    if shutil.which("lscpu") is not None:
        listed = subprocess.run(("lscpu",), capture_output=True, text=True, check=False)
        fields: dict[str, str] = {}
        for line in listed.stdout.splitlines():
            name, _, value = line.partition(":")
            if value.strip():
                fields.setdefault(name.strip().lower(), value.strip())
        if "model name" in fields:
            return fields["model name"]
        if "vendor id" in fields:
            return f"{fields['vendor id']}, model not published by the kernel"

    return platform.processor() or "not published by the kernel"


def memory() -> str:
    """Total memory, as the kernel reports it."""
    info = Path("/proc/meminfo")
    if not info.is_file():
        return ""
    match = re.search(r"^MemTotal:\s+(\d+) kB", info.read_text(encoding="utf-8"), re.M)
    if match is None:
        return ""
    return f"{int(match.group(1)) / MEBIBYTE / MEBIBYTE:.1f} GiB"


def processors() -> str:
    """How many processors the run could use, and the quota when one is imposed.

    A container is often given less than it can see, and a duration means nothing without it.
    """
    # The affinity mask is what a process may actually use and is Linux-only; elsewhere the
    # processor count is the closest thing the platform offers.
    affinity = getattr(os, "sched_getaffinity", None)
    seen = str(len(affinity(0))) if affinity is not None else str(os.cpu_count() or "")

    quota = Path("/sys/fs/cgroup/cpu.max")
    if quota.is_file():
        parts = quota.read_text(encoding="utf-8").split()
        if len(parts) == 2 and parts[0] != "max":
            return f"{seen} ({int(parts[0]) / int(parts[1]):.1f} allowed by quota)"
    return seen


def build_facts(build_dir: Path) -> list[tuple[str, str]]:
    """What the build itself says it is, read from its cache.

    The alternative is a string on a command line, which is how this report came to claim C++23
    on an arm64 machine for a C++17 build on x86_64. The cache cannot be wrong about its own
    build.
    """
    cache = build_dir / "CMakeCache.txt"
    toolchain = build_dir / "generators" / "conan_toolchain.cmake"
    cache_text = cache.read_text(encoding="utf-8", errors="replace") if cache.is_file() else ""
    toolchain_text = toolchain.read_text(encoding="utf-8", errors="replace") if toolchain.is_file() else ""
    if not cache_text and not toolchain_text:
        return []

    # The build type is a cache entry; the language standard is not, because conan sets it in the
    # toolchain file instead. Looking in one place only would drop it without saying so.
    version = re.search(r"^CMAKE_PROJECT_VERSION:\w+=(.*)$", cache_text, re.M)
    build_type = re.search(r"^CMAKE_BUILD_TYPE:\w+=(.*)$", cache_text, re.M)
    standard = re.search(r"set\(CMAKE_CXX_STANDARD (\d+)\)", toolchain_text) or re.search(
        r"^CMAKE_CXX_STANDARD:\w+=(.*)$", cache_text, re.M
    )
    return [
        ("Sen version", version.group(1).strip() if version else ""),
        ("Build type", build_type.group(1).strip() if build_type else ""),
        ("C++ standard", f"C++{standard.group(1).strip()}" if standard else ""),
    ]


def dependencies(lockfile: Path) -> str:
    """Identifies the dependency set, rather than listing it.

    Every third-party package is pinned in the lockfile by version and recipe revision, so its
    digest names the exact set a result was produced against and stays resolvable for as long as
    the lockfile is in the history. Enumerating fifty-four packages here would put a bill of
    materials inside a test report, which is a different document for a different reader.
    """
    if not lockfile.is_file():
        return "no lockfile"

    raw = lockfile.read_bytes()
    try:
        locked = json.loads(raw)
    except json.JSONDecodeError:
        return "lockfile unreadable"

    pinned = len(locked.get("requires", [])) + len(locked.get("build_requires", []))
    return f"{pinned} packages, {lockfile.name} {hashlib.sha256(raw).hexdigest()[:12]}"


def node_version(build_dir: Path | None) -> str:
    """The Node the run actually used.

    It arrives as a conan package and is never on PATH, so asking the PATH reports it missing on a
    machine that just ran five Node suites. The build recorded where it is; that is the answer.
    """
    if build_dir is not None:
        cache = build_dir / "CMakeCache.txt"
        if cache.is_file():
            match = re.search(r"^NPM_EXEC:\w+=(.*)$", cache.read_text(encoding="utf-8", errors="replace"), re.M)
            if match:
                beside = Path(match.group(1).strip()).parent / "node"
                if beside.is_file():
                    return first_line((str(beside), "--version"))

    return first_line(("node", "--version")) or "not installed"


def facts(compiler: str, build_dir: Path | None = None, lockfile: Path | None = None) -> list[tuple[str, str]]:
    """Everything worth recording about where a run happened."""
    recorded = list(build_facts(build_dir) if build_dir is not None else [])
    recorded += [
        ("Operating system", distribution()),
        ("Kernel", f"{platform.system()} {platform.release()}"),
        ("Architecture", platform.machine()),
        ("Processor", processor()),
        ("Processors available", processors()),
        ("Memory", memory()),
        ("Host", platform.node()),
    ]
    if compiler:
        recorded.append(("Compiler", first_line((compiler, "--version")) or compiler))
    recorded.extend((name, first_line(command)) for name, command in TOOLS)
    recorded.append(("Node", node_version(build_dir)))
    if lockfile is not None:
        recorded.append(("Dependencies", dependencies(lockfile)))
    return [(name, value or "not reported") for name, value in recorded]


def main() -> int:
    """Writes the environment of this machine to the file named on the command line."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="where to write the record")
    parser.add_argument("--compiler", default="", help="the compiler the run was built with")
    parser.add_argument("--build-dir", type=Path, default=None, help="the build whose cache to read")
    parser.add_argument("--lockfile", type=Path, default=Path(".conan/conan.lock"), help="the pinned dependency set")
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        "".join(f"{name}\t{value}\n" for name, value in facts(args.compiler, args.build_dir, args.lockfile)),
        encoding="utf-8",
    )
    print(f"recorded the environment to {args.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
