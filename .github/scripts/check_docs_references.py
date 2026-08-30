# === check_docs_references.py =========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Fails when source code cites a `docs/` path that does not exist.

mkdocs `strict: true` validates links *inside* the site. It cannot see a path
written in a `.cpp` comment, a `.ts` config, a shell script or a CMake file, so
that whole class of reference rots silently. One did: a vite config pointed at a
findings document that was never committed to any branch, under a directory that
only ever existed on an abandoned one. Nothing noticed, because nothing looked.

These references matter more than they look. A shipped binary prints one to end
users (`libs/db/bindings/python/module.cpp`, via `help()`), one carries a
maintenance contract naming a page that must stay in sync
(`libs/core/src/meta/unit_registry.cpp`), and the installer users `curl` cites
another.

Three things are deliberately *not* reported, each of which produced a false
positive on the first run:

- **A path nested under another directory**, so a build output under
  `build/<compiler>/<config>/docs/site/` is not mistaken for the source tree. A
  reference counts when it starts the path, or when it follows a CMake variable
  expansion such as `${PROJECT_SOURCE_DIR}`.
- **Anything inside a URL.** A C++ standards paper URL happens to contain the
  same three letters and a slash.
- **Test files.** They feed synthetic paths to path classifiers, which are not
  supposed to exist, so checking them would fail the build on correct code. The
  cost is that a rotten reference inside a test goes unseen, which is the right
  trade: tests do not ship.

Note that `mkdocs/` ends with the same four characters, so the match must begin
at a token boundary or every file under `libs/gen/src/mkdocs/` is reported.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent

SUFFIXES = {
    ".c",
    ".cpp",
    ".h",
    ".hpp",
    ".ts",
    ".tsx",
    ".js",
    ".py",
    ".sh",
    ".cmake",
    ".j2",
    ".yaml",
    ".yml",
    ".stl",
    ".txt",
}

# docs/ itself is mkdocs' job; the rest hold no first-party source.
SKIP_DIRS = {".git", "node_modules", "build", "docs", ".workplan", "__pycache__", ".venv"}

# See the module docstring: synthetic paths in tests are inputs, not references.
TEST_PATHS = re.compile(r"(^test_|_test\.|\.bats$)")
TEST_DIRS = {"test", "tests"}

# `(?<![A-Za-z0-9_-])` keeps `mkdocs/` from matching. A separator must follow, so
# prose like "the docs/ tree" is not mistaken for a path.
REFERENCE = re.compile(r"(?<![A-Za-z0-9_-])docs/[A-Za-z0-9_.@/-]*[A-Za-z0-9_]")


def is_repo_relative(line, start):
    """True when this `docs/` starts a path rather than sitting inside one.

    `build/gcc/Release/docs/site` is a build output, not the source tree, and
    `.../wg21/docs/papers/...` is a URL. Both are preceded by a path segment.
    A CMake variable expansion is the exception: `${PROJECT_SOURCE_DIR}/docs/`
    really does name the source tree.
    """
    before = line[:start]
    if "://" in before:
        return False
    if not before.endswith("/"):
        return True
    return before[:-1].endswith(("}", ")"))


def source_files(root):
    """Every first-party file that could carry a path literal."""
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if set(relative.parts[:-1]) & SKIP_DIRS:
            continue
        if path.suffix not in SUFFIXES and path.name != "CMakeLists.txt":
            continue
        if TEST_PATHS.search(path.name) or set(relative.parts[:-1]) & TEST_DIRS:
            continue
        yield path


# Screenshots are hosted on a side branch and linked by raw URL, on purpose: it
# keeps several megabytes of binaries out of the history that everyone clones,
# without git-lfs, whose storage endpoints some corporate networks block.
#
# The cost of that arrangement is that nothing validates the links. mkdocs
# `strict: true` does not check external URLs, so a renamed or removed image
# fails only in the browser. This checks each one against the branch as fetched,
# which needs no network beyond a ref the clone may already have.
HOSTED_IMAGE = re.compile(
    r"raw\.githubusercontent\.com/[A-Za-z0-9_.-]+/sen/refs/heads/"
    r"(?P<ref>[A-Za-z0-9_./-]+?)/(?P<file>[A-Za-z0-9_.-]+\.(?:gif|png|jpe?g|webp|svg))"
)


def _ref_listing(root, ref):
    """File names in a fetched ref, or None when it is not present.

    `cwd=root` matters: without it this reads whichever repository the process
    happens to be standing in, so a scan of some other tree silently answers
    from this one.
    """
    for candidate in (f"origin/{ref}", ref):
        result = subprocess.run(
            ["git", "ls-tree", "-r", "--name-only", candidate],
            capture_output=True,
            text=True,
            check=False,
            cwd=root,
        )
        if result.returncode == 0:
            return set(result.stdout.split())
    return None


def hosted_images(root):
    """Hotlinked screenshots whose file is missing from the branch hosting them.

    Returns (problems, skipped_refs). A ref that is not fetched is skipped rather
    than reported: a shallow CI checkout does not have it, and failing there
    would punish a correct tree.
    """
    problems, skipped, listings = [], set(), {}
    for path in sorted((root / "docs").rglob("*.md")):
        if "snippets" in path.relative_to(root).parts:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            for match in HOSTED_IMAGE.finditer(line):
                ref, name = match.group("ref"), match.group("file")
                if ref not in listings:
                    listings[ref] = _ref_listing(root, ref)
                if listings[ref] is None:
                    skipped.add(ref)
                    continue
                if name not in listings[ref]:
                    problems.append((path.relative_to(root).as_posix(), number, f"{ref}:{name}"))
    return problems, sorted(skipped)


# A page can include a named region of real compiled source, so the sample cannot
# drift from the code it demonstrates. The cost is a marker living in a file
# whose author has no reason to expect one, so this checks the pair still exists.
SNIPPET = re.compile(r'--8<--\s+"([^":]+):([A-Za-z0-9_.-]+)"')


def snippet_sections(root):
    """Includes naming a region that its file does not mark, as (file, line, ref)."""
    problems = []
    for path in sorted((root / "docs").rglob("*.md")):
        if "snippets" in path.relative_to(root).parts:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            for target, section in SNIPPET.findall(line):
                # mkdocs searches docs/ first, then the repository root.
                for base in (root / "docs", root):
                    source = base / target
                    if source.is_file():
                        break
                else:
                    problems.append((path.relative_to(root).as_posix(), number, f"{target} (no such file)"))
                    continue
                if f"[start:{section}]" not in source.read_text(encoding="utf-8", errors="ignore"):
                    problems.append((path.relative_to(root).as_posix(), number, f"{target}:{section}"))
    return problems


def dangling(root):
    """Every cited docs/ path that is not on disk, as (file, line, path)."""
    problems = []
    for path in source_files(root):
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for number, line in enumerate(text.splitlines(), 1):
            for match in REFERENCE.finditer(line):
                cited = match.group()
                if not is_repo_relative(line, match.start()):
                    continue
                if not (root / cited).exists():
                    problems.append((path.relative_to(root).as_posix(), number, cited))
    return problems


def main():
    """Reports both kinds of rotten reference, and fails if either is found."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", default=str(ROOT), help="repository root to scan")
    args = parser.parse_args()

    root = Path(args.root).resolve()

    problems = dangling(root)
    for file, number, cited in problems:
        print(f"{file}:{number}: cites `{cited}`, which does not exist")

    sections = snippet_sections(root)
    for file, number, ref in sections:
        print(f"{file}:{number}: includes `{ref}`, which is not marked in that file")

    missing, skipped = hosted_images(root)
    for file, number, where in missing:
        print(f"{file}:{number}: hotlinked image `{where}` is not on that branch")
    for ref in skipped:
        print(f"note: `{ref}` is not fetched, so its images were not checked (git fetch origin {ref} --depth 1)")

    if problems or missing or sections:
        print(
            f"\n{len(problems)} dangling reference(s), {len(sections)} unmarked snippet section(s), "
            f"{len(missing)} missing hosted image(s)"
        )
        return 1
    print("documentation references: all resolve")
    return 0


if __name__ == "__main__":
    sys.exit(main())
