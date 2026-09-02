# === check.py =========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Structural check of the TypeScript the `ts` generator emits.

Replaces nine CTest `PASS_REGULAR_EXPRESSION` checks that asked whether a sentinel appeared
anywhere in the output. Measured, not supposed: all nine passed against a file whose contents
were `export type Direction = nonsense` and `export interface Point  --- garbage ---`. Presence
of a substring says nothing about the declaration it came from.

Three of the eight constructs here are checked nowhere else. `tsc --noEmit` covers the client's
generated types under `strict`, but that is a different invocation over `jsonrpc.stl`, which
declares only class, optional, sequence, struct and variant. `alias`, `enum` and `quantity` are
emitted solely by this fixture, so these assertions are their only test.

Compares whole declarations rather than fragments, so a body that changes shape fails even when
its name and keywords survive. stdlib only: making cli_gen's tests require a node toolchain to
typecheck its own output would be a worse trade than asserting the text.
"""

import os
import re
import subprocess
import sys

# Whole declarations, whitespace-normalised. Keyed by the construct each one exercises, so a
# failure names the template that drifted rather than only the symbol.
EXPECTED: dict[str, tuple[str, str]] = {
    "enum": ("Direction", 'export type Direction = | "north" | "east" | "south" | "west" ;'),
    "struct": ("Point", "export interface Point { x: number; y: number; }"),
    "struct inheritance": ("Point3D", "export interface Point3D extends Point { z: number; }"),
    "sequence": ("Path", "export type Path = Point[];"),
    "optional": ("MaybeDirection", "export type MaybeDirection = Direction | null;"),
    "alias": ("ObjectId", "export type ObjectId = number;"),
    "quantity": ("Altitude", "export type Altitude = number;"),
    "variant": (
        "Shape",
        'export type Shape = | { type: "test.ts.Point"; value: Point } | { type: "test.ts.Point3D"; value: Point3D } ;',
    ),
    "class event": ("ReadingNotification", "export interface ReadingNotification { value: number; }"),
    "class event with enum arg": (
        "TriggeredNotification",
        "export interface TriggeredNotification { reason: string; severity: Direction; }",
    ),
}


def declarations(source: str) -> dict[str, str]:
    """Split the emitted module into whole declarations, keyed by the symbol each one declares."""
    out: dict[str, str] = {}
    for raw in re.split(r"\n\s*\n", source):
        block = raw.strip()
        if not block or block.startswith("//"):
            continue
        match = re.match(r"export (?:type|interface)\s+([A-Za-z0-9_]+)", block)
        if match:
            # Drop trailing line comments before normalising: they are documentation carried from
            # the STL, and asserting on them would make every comment edit a test failure.
            body = re.sub(r"//[^\n]*", "", block)
            out[match.group(1)] = " ".join(body.split())
    return out


def main() -> None:
    """Render the fixture and compare each emitted declaration against its expected form."""
    source_dir = os.path.dirname(os.path.abspath(__file__))
    out_dir = os.path.join(os.getcwd(), "ts_structural")
    cmd = ["./cli_gen", "ts", "stl", os.path.join(source_dir, "fixture.stl"), "-d", out_dir]
    print(f"Executing: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        sys.exit(f"cli_gen failed ({result.returncode}):\n{result.stdout}\n{result.stderr}")

    emitted = os.path.join(out_dir, "fixture.ts")
    if not os.path.isfile(emitted):
        sys.exit(f"generator reported success but produced no {emitted}")
    with open(emitted, encoding="utf-8") as handle:
        found = declarations(handle.read())

    failures: list[str] = []
    for construct, (symbol, expected) in EXPECTED.items():
        actual = found.get(symbol)
        if actual is None:
            failures.append(f"{construct}: no declaration of {symbol}; emitted {sorted(found)}")
        elif actual != expected:
            failures.append(f"{construct}: {symbol}\n      expected: {expected}\n      actual:   {actual}")

    if failures:
        print(f"\n{len(failures)} declaration(s) did not match:", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        sys.exit(1)
    print(f"typescript structural checks passed ({len(EXPECTED)} declarations over 8 constructs)")


if __name__ == "__main__":
    main()
