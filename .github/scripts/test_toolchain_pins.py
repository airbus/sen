"""Guard the tool versions that are pinned by hand in more than one place.

conan, pytest and junitparser are pinned in the workflows, in the composite
actions and in the image; cmake is pinned in the image and required from Conan.
Nothing kept them in agreement, and `tools/ci/Dockerfile` only asked that the
image and the runner setup be "kept in sync" by hand.

A divergence here is the quiet kind: the runner installs one version, the image
carries another, and a build behaves differently depending on where it ran.
"""

import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCKERFILE = ROOT / "tools" / "ci" / "Dockerfile"
CONANFILE = ROOT / "conanfile.py"
PRECOMMIT = ROOT / ".pre-commit-config.yaml"

# `name==1.2.3`, the pip form used across the workflows and actions.
PIN = re.compile(r"\b([a-z0-9_-]+)==([0-9][0-9a-z.]*)")
# `name==${ARG}` in the image, which needs the ARG resolved first.
IMAGE_PIN = re.compile(r"\b([a-z0-9_-]+)==\$\{([A-Z_]+)\}")
ARG = re.compile(r"^ARG ([A-Z_]+)=(.+)$", re.M)
TOOL_REQUIRE = re.compile(r'tool_requires\(\s*"([a-z0-9_-]+)/([0-9][0-9a-z.]*)"')


def pins() -> dict[str, dict[str, list[str]]]:
    """Maps package -> version -> the places that pin it."""
    found: dict[str, dict[str, list[str]]] = defaultdict(lambda: defaultdict(list))

    sources = sorted((ROOT / ".github").rglob("*.yaml"))
    for path in sources:
        for name, version in PIN.findall(path.read_text(encoding="utf-8")):
            found[name][version].append(str(path.relative_to(ROOT)))

    text = DOCKERFILE.read_text(encoding="utf-8")
    args = dict(ARG.findall(text))
    for name, arg in IMAGE_PIN.findall(text):
        if arg in args:
            found[name][args[arg]].append(f"{DOCKERFILE.relative_to(ROOT)} (ARG {arg})")

    # clang-format comes from a pre-commit mirror rather than the image, and only
    # its major is meant to track the toolchain. Recorded as that major so it can
    # be compared with LLVM_VERSION rather than against a full patch version.
    mirror = re.search(r"mirrors-clang-format\s*\n\s*rev:\s*v?([0-9]+)", PRECOMMIT.read_text(encoding="utf-8"))
    if mirror:
        found["clang-major"][mirror.group(1)].append(f"{PRECOMMIT.relative_to(ROOT)} (mirrors-clang-format rev)")
    if "LLVM_VERSION" in args:
        found["clang-major"][args["LLVM_VERSION"]].append(f"{DOCKERFILE.relative_to(ROOT)} (ARG LLVM_VERSION)")

    # Conan requires cmake and ninja separately from the image's own install.
    for name, version in TOOL_REQUIRE.findall(CONANFILE.read_text(encoding="utf-8")):
        found[name][version].append(f"{CONANFILE.relative_to(ROOT)} (tool_requires)")

    return found


def test_every_tool_is_pinned_to_one_version():
    """A tool pinned in several places must name the same version in all of them."""
    disagreements = []
    for name, versions in sorted(pins().items()):
        if len(versions) > 1:
            detail = "; ".join(f"{v} in {', '.join(sorted(set(where)))}" for v, where in sorted(versions.items()))
            disagreements.append(f"{name}: {detail}")

    assert not disagreements, "tool versions disagree -- " + " | ".join(disagreements)


def test_the_guard_covers_the_tools_it_was_written_for():
    """Without this, a rename would empty the check and it would still pass."""
    found = pins()
    for name in ("conan", "pytest", "junitparser", "cmake", "clang-major"):
        assert name in found, f"{name} is no longer found by the pin scan"
        # Distinct files, not sites: one file naming the same version twice is
        # one fact, and counting it twice let this guard pass over an empty
        # comparison.
        files = {w.split(" (")[0] for where in found[name].values() for w in where}
        assert len(files) >= 2, f"{name} is pinned in only {files}, so nothing is compared"
