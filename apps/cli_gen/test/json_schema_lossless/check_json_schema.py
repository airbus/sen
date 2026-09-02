# === check.py =========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Structural check of the JSON-Schema the `json package` generator emits.

Replaces thirteen CTest `PASS_REGULAR_EXPRESSION` checks that matched substrings in document
order. Those could not tell schema output from prose: a file containing only
`{"nonsense": "x-enum ... idle ... : 0"}` satisfied the enum check, and three lines of text
holding `allOf` and `schema_lossless.Base` satisfied the inheritance one. Assertions here
address values by JSON path, so a key that moves to the wrong object fails.

These fragments are not a build artefact. `libs/gen`'s JsonGenerator is linked into the
jsonrpc component and renders them at runtime for `getType(withSchema)` and
`createInterest(withSchemas)`, so this is a wire format.

Runs the generator itself rather than reading a file another test produced, matching test19
and test20. The previous design had a render test writing into the shared binary directory and
thirteen tests reading it back, ordered only by DEPENDS, which sequences but does not gate.
"""

import json
import os
import subprocess
import sys
from typing import Any

PKG = "schema_lossless"

# (path, expected, why) -- `why` names the fixture comment the assertion enforces, so a failure
# says what the generator was supposed to preserve rather than only which key moved.
CHECKS: list[tuple[list, Any, str]] = [
    # Enum: integer values preserved alongside names.
    ([f"{PKG}.Status", "enum"], ["idle", "active", "error"], "enum names, in declaration order"),
    ([f"{PKG}.Status", "x-enum"], {"idle": 0, "active": 1, "error": 2}, "enum integer values"),
    # Quantity: unit metadata preserved.
    ([f"{PKG}.Meters", "x-unit"], "m", "quantity unit"),
    ([f"{PKG}.Meters", "minimum"], 0.0, "quantity lower bound"),
    ([f"{PKG}.Meters", "maximum"], 1000.0, "quantity upper bound"),
    ([f"{PKG}.Meters", "x-element-type", "type"], "integer", "quantity element type"),
    # Duration renders as integer; TimeStamp as a date-time string.
    ([f"{PKG}.Timing", "properties", "delay", "type"], "integer", "Duration is an integer"),
    ([f"{PKG}.Timing", "properties", "when", "type"], "string", "TimeStamp is a string"),
    ([f"{PKG}.Timing", "properties", "when", "format"], "date-time", "TimeStamp carries a format"),
    # Inheritance: the child references the parent, and by the parent's own $id.
    ([f"{PKG}.Derived", "allOf", 0, "$ref"], f"{PKG}.Base", "child references parent"),
    ([f"{PKG}.Base", "$id"], f"{PKG}.Base", "parent $id is what the child references"),
    ([f"{PKG}.Derived", "properties", "derivedField", "type"], "integer", "own field survives"),
    # Class: categories, statics, methods, events.
    ([f"{PKG}.SchemaLosslessClass", "properties", "staticVersion", "x-category"], "staticRW", "static category"),
    ([f"{PKG}.SchemaLosslessClass", "properties", "counter", "x-category"], "dynamicRW", "dynamic category"),
    ([f"{PKG}.SchemaLosslessClass", "x-is-interface"], False, "class is not an interface"),
]


def dig(root: Any, path: list) -> Any:
    """Walk `path`, raising KeyError with the path prefix that failed rather than a bare miss."""
    node = root
    for i, step in enumerate(path):
        try:
            node = node[step]
        except (KeyError, IndexError, TypeError) as exc:
            raise KeyError(f"{' -> '.join(map(str, path[: i + 1]))} ({type(exc).__name__})") from exc
    return node


def render(source_dir: str, out: str) -> dict:
    """Run the generator over the fixture and return the parsed document."""
    cmd = ["./cli_gen", "json", "package", "stl", os.path.join(source_dir, "fixture.stl"), "-o", out]
    print(f"Executing: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        sys.exit(f"cli_gen failed ({result.returncode}):\n{result.stdout}\n{result.stderr}")
    with open(out, encoding="utf-8") as handle:
        return json.load(handle)


def document_shape(doc: dict) -> list[str]:
    """Assertions about the document itself rather than any one definition.

    The dialect belongs on the document, not on every nested definition: a subschema that
    redeclares $schema is the bug this generator's $id-per-definition shape avoids.
    """
    problems = []
    if "$schema" not in doc:
        problems.append("root: $schema absent -- consumers cannot resolve the dialect")
    for name, body in doc.get("$defs", {}).items():
        if isinstance(body, dict) and "$schema" in body:
            problems.append(f"$defs.{name}: nested definitions must not redeclare $schema")
    return problems


def main() -> None:
    """Render the fixture and assert the emitted schema by JSON path."""
    source_dir = os.path.dirname(os.path.abspath(__file__))
    doc = render(source_dir, os.path.join(os.getcwd(), "lossless_structural.json"))
    defs = doc.get("$defs")
    if not isinstance(defs, dict):
        sys.exit(f"$defs missing or not an object; got {type(defs).__name__}")
    failures: list[str] = document_shape(doc)

    # Every assertion runs; reporting only the first failure hides how far output has drifted.
    for path, expected, why in CHECKS:
        try:
            actual = dig(defs, path)
        except KeyError as exc:
            failures.append(f"{why}: path not found: {exc}")
            continue
        if actual != expected:
            failures.append(f"{why}: at {' -> '.join(map(str, path))} expected {expected!r}, got {actual!r}")

    if failures:
        print(f"\n{len(failures)} structural check(s) failed:", file=sys.stderr)
        for failure in failures:
            print(f"  - {failure}", file=sys.stderr)
        sys.exit(1)
    print(f"json schema structural checks passed ({len(CHECKS)} assertions over {len(defs)} definitions)")


if __name__ == "__main__":
    main()
