#!/usr/bin/env python3
# === stage_doc_placeholders.py ========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Fills the gaps a no-build documentation preview leaves.

Some of the site is written by running the Sen binaries. `docs_preview` does not
build them, so it stands placeholders in their place. The files to stand in for
are named by the caller, which already knows them, so this holds no list of its
own to fall out of step.

Only gaps are filled: a file already on disk is left alone.
"""

import argparse
import os
import sys
from pathlib import Path

NOTE = "the documentation is built with the Sen binaries present. docs_preview does not build them."

# Dated far enough back that any real build looks newer. Without this a preview
# could leave a placeholder that a later build treats as up to date and
# publishes as if it were genuine output.
STALE = (0, 946684800)  # 2000-01-01


def content_for(path):
    """A stand-in whose shape suits where the file is used."""
    if path.suffix == ".svg":
        return (
            '<svg xmlns="http://www.w3.org/2000/svg" width="620" height="80" role="img"\n'
            f'     aria-label="Placeholder for {path.name}">\n'
            '  <rect width="620" height="80" fill="#eeeeee"/>\n'
            '  <text x="20" y="45" font-family="sans-serif" font-size="15" fill="#333333">\n'
            f"    Placeholder: {path.name} is drawn when the documentation is built.\n"
            "  </text>\n"
            "</svg>\n"
        )

    if path.suffix == ".html":
        # The API reference, which doxygen writes and the handbook links to.
        return (
            '<!DOCTYPE html>\n<html lang="en"><head><meta charset="utf-8">\n'
            f"<title>{path.stem}</title></head><body>\n"
            f"<h1>{path.stem}</h1>\n<p>Placeholder. The API reference is written when {NOTE}</p>\n"
            "</body></html>\n"
        )

    if path.suffix == ".md":
        # A nav page, so it needs a heading of its own.
        title = path.stem.replace("_", " ").replace("-", " ").capitalize()
        return f"# {title}\n\nPlaceholder. The real page is written when {NOTE}\n"

    # Included inside a fenced block, so plain text is what fits.
    return f"Placeholder. The real text is written when {NOTE}\n"


def main():
    """Returns 0 unless a named path lies outside the documentation tree."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--docs", required=True, type=Path, help="the documentation tree")
    parser.add_argument("paths", nargs="+", type=Path, help="files the binaries would have written")
    args = parser.parse_args()

    docs = args.docs.resolve()
    filled = 0
    for path in args.paths:
        resolved = path.resolve()
        if docs not in resolved.parents:
            print(f"{path} is outside {args.docs}", file=sys.stderr)
            return 1
        if resolved.exists():
            continue
        resolved.parent.mkdir(parents=True, exist_ok=True)
        resolved.write_text(content_for(resolved), encoding="utf-8")
        os.utime(resolved, STALE)
        filled += 1

    print(f"documentation preview: {filled} of {len(args.paths)} file(s) stood in for, the rest were built")
    return 0


if __name__ == "__main__":
    sys.exit(main())
