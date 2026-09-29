# === check_file_banner.py =============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Checks that a source file opens with the licence banner.

It carries the SPDX identifier, the copyright and the file's own name.
"""

import argparse
import pathlib
import sys

WIDTH = 120

# The comment marker each kind of file opens with. A suffix absent from here is not checked.
PREFIX_BY_SUFFIX = {
    ".cpp": "//",
    ".h": "//",
    ".ts": "//",
    ".tsx": "//",
    ".cmake": "#",
    ".py": "#",
    ".bats": "#",
}

# By name, not by suffix: LICENSE.txt and the sanitizer ignorelists share `.txt` and carry none.
PREFIX_BY_NAME = {"CMakeLists.txt": "#"}

# The text sits at the same offset from the comment marker in both forms, so a `//` line is one
# column wider than a `#` one.
BODY = (
    "{c}                                               Sen Infrastructure",
    "{c}                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).",
    "{c}                                    See the LICENSE.txt file for more information.",
    "{c}                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.",
)


def expected_banner(name: str, comment: str) -> list[str]:
    """The six lines the file should open with, padded to the repository's column."""
    opening = f"{comment} === {name} "
    closing = f"{comment} "
    return [
        opening + "=" * (WIDTH - len(opening)),
        *(line.format(c=comment) for line in BODY),
        closing + "=" * (WIDTH - len(closing)),
    ]


def check(path: pathlib.Path) -> str:
    """Empty when the file is fine, otherwise what is wrong with it."""
    comment = PREFIX_BY_NAME.get(path.name) or PREFIX_BY_SUFFIX.get(path.suffix)
    if comment is None:
        return ""

    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines:
        return "the file is empty"

    # A shebang has to stay on the first line, so the banner follows it rather than displacing it.
    start = 1 if lines[0].startswith("#!") else 0
    want = expected_banner(path.name, comment)
    got = lines[start : start + len(want)]
    if got == want:
        return ""

    # Padded so a file shorter than the banner still reports the first line that differs.
    padded = (got + [""] * len(want))[: len(want)]
    for offset, (a, b) in enumerate(zip(want, padded, strict=True)):
        if a != b:
            return f"line {start + offset + 1} should be:\n    {a}\n  and is:\n    {b}"
    return "the banner is missing"


def main() -> int:
    """Checks each named file and names the first line of any banner that is wrong."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=pathlib.Path)
    args = parser.parse_args()

    bad = 0
    for path in args.paths:
        problem = check(path)
        if problem:
            bad += 1
            print(f"{path}: {problem}", file=sys.stderr)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
