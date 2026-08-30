#!/usr/bin/env python3
# === generate_license_page.py =========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Writes the third-party licence page from the baked licence tree.

The tree is `<dir>/<consumer>/<dependency>/<licence files>`. conanfile.py fills
the C++ half during `conan install`; the npm half is baked by a build target,
which is why this runs at build time rather than while CMake configures.
"""

import argparse
import sys
from pathlib import Path

PREAMBLE = """# Licensed Code in Sen

The public API of Sen **has no dependencies**.

Internally, some parts of the Sen _implementation_ uses third-party
code that is licensed under specific open-source licenses from the
original authors:

"""


def consumers(root):
    """Consumer directories that actually hold a dependency, in a stable order.

    A consumer with no dependency underneath would render as a heading with
    nothing below it.
    """
    for consumer in sorted(p for p in root.iterdir() if p.is_dir()):
        if any(dep.is_dir() for dep in consumer.iterdir()):
            yield consumer


def render(root):
    """The page, as text."""
    parts = [PREAMBLE]
    for consumer in consumers(root):
        parts.append(f"## {consumer.name}\n\n")
        for dep in sorted(p for p in consumer.iterdir() if p.is_dir()):
            parts.append(f"### {dep.name}\n")
            for licence in sorted(p for p in dep.rglob("*") if p.is_file()):
                body = licence.read_text(encoding="utf-8", errors="replace")
                parts.append(f'\n```text title="{licence.name}"\n{body}\n```\n\n')
    return "".join(parts)


def main():
    """Returns 0 on success, 1 when there is nothing to attribute."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--licenses-dir", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    if not args.licenses_dir.is_dir():
        print(f"{args.licenses_dir} does not exist, so no licence could be attributed.", file=sys.stderr)
        return 1

    page = render(args.licenses_dir)
    # An empty page would claim Sen ships no third-party code, which is false.
    if "## " not in page:
        print(f"{args.licenses_dir} holds no dependency licences.", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(page, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
