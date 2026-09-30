#!/usr/bin/env python3
# === format_stl.py ====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Aligns the members of a compound type and the comments beside them.

Only whitespace between tokens ever changes, so a file this cannot read comes back untouched
rather than damaged.
"""

import argparse
import pathlib
import re
import sys

# A run is a block of consecutive members. One held apart by its own doc comment is a run of one,
# and keeps the spacing it has.
MEMBER = re.compile(
    r"""^(?P<indent>\s*)
         (?P<head>(?:var\s+)?[A-Za-z_]\w*)
         (?:\s*:\s*(?P<type>[^\[\],;/]+?))?
         \s*(?P<attrs>\[[^\]]*\])?
         \s*(?P<term>[,;])?
         \s*(?P<comment>//.*)?$""",
    re.VERBOSE,
)


def split_member(line: str) -> dict[str, str] | None:
    """The fields of a member declaration, or None when the line is something else."""
    # A quoted string can hold a colon or a comment marker, and splitting on those would move text
    # rather than whitespace.
    if '"' in line:
        return None
    match = MEMBER.match(line.rstrip())
    if match is None or not line.strip():
        return None
    parts = {k: (v or "").strip() for k, v in match.groupdict().items()}
    parts["indent"] = match.group("indent")
    # An identifier alone is a keyword or a stray word, not a member; an enumerator has a comma.
    if not parts["type"] and not parts["term"] and not parts["comment"]:
        return None
    return parts


def body_of(parts: dict[str, str], head_width: int) -> str:
    """The member without its comment, with the colon in its column.

    An attribute block follows its member by a single space rather than taking a column of its own.
    """
    if not parts["type"]:
        return parts["indent"] + parts["head"] + parts["term"]
    body = f"{parts['indent']}{parts['head']:<{head_width}} : {parts['type']}"
    if parts["attrs"]:
        body += " " + parts["attrs"]
    return body + parts["term"]


def align_run(run: list[dict[str, str]]) -> list[str]:
    """The run with its colon and its comment each in one column."""
    head_width = max(len(p["head"]) for p in run)
    bodies = [body_of(p, head_width) for p in run]
    comment_column = max(len(b) for b in bodies)
    return [
        f"{body:<{comment_column}}  {parts['comment']}".rstrip() if parts["comment"] else body
        for body, parts in zip(bodies, run, strict=True)
    ]


# Anything that declares a member. A package or import line sits outside a compound type, and
# braces and comments declare nothing.
MEMBER_LINE = re.compile(r"^(?:var|fn|event)\s+\w|^\w+\s*[:,;]|^\w+,?$")


def is_member_line(line: str) -> bool:
    """Whether the line declares a member of the compound type being read."""
    stripped = line.strip()
    if not stripped or stripped.startswith(("//", "{", "}")):
        return False
    return bool(MEMBER_LINE.match(stripped))


def is_ruler(stripped: str) -> bool:
    """A separator rather than documentation, which the scanner discards without a token."""
    return stripped.startswith("//--") or stripped.startswith("// --")


def is_marker(stripped: str) -> bool:
    """A transclusion marker, which delimits the region the documentation pulls in."""
    return "--8<--" in stripped


def is_doc_comment(stripped: str) -> bool:
    """A comment that says something about the declaration below it."""
    return stripped.startswith("//") and not is_ruler(stripped)


def separate_documented_members(lines: list[str]) -> list[str]:
    """A blank line before a member's own comment, so the comment reads as belonging below it."""
    out: list[str] = []
    depth = 0
    for index, line in enumerate(lines):
        stripped = line.strip()
        if not is_doc_comment(stripped):
            if not stripped.startswith("//"):
                depth += line.count("{") - line.count("}")
            out.append(line)
            continue

        starts_block = index == 0 or not is_doc_comment(lines[index - 1].strip())
        # A marker has to stay against what it marks, or the region the documentation pulls in
        # would start with a blank line.
        above = out[-1].strip() if out else ""
        if depth > 0 and starts_block and out and above not in ("", "{") and not is_marker(above):
            end = index
            while end < len(lines) and is_doc_comment(lines[end].strip()):
                end += 1
            if end < len(lines) and is_member_line(lines[end]):
                out.append("")
        out.append(line)
    return out


def one_space_before_attributes(line: str) -> str:
    """An attribute block follows what it applies to by a single space, whatever the line is.

    A member on its own, a function and an event all carry attributes and none of them line up.
    """
    if '"' in line:
        return line
    code, marker, comment = line.partition("//")
    return re.sub(r"(\S)[ \t]+\[", r"\1 [", code) + marker + comment


SIGNATURE = re.compile(r"^\s*(?:fn|event)\s+\w+\s*\(")
PARAMETER = re.compile(r"([A-Za-z_]\w*)( *): ")


def code_of(line: str) -> str:
    """The line without its trailing comment, which a closing bracket can hide behind."""
    return line.split("//", maxsplit=1)[0].rstrip()


def opens_a_signature(line: str) -> bool:
    """Whether the line starts a parameter list that a later line closes."""
    code = code_of(line)
    return bool(SIGNATURE.match(code)) and code.count("(") > code.count(")")


def align_signature(block: list[str]) -> list[str]:
    """A wrapped parameter list, indented under its bracket with the colons in one column.

    The parameter on the first line counts: without it the continuation lines line up with each
    other and not with the one they follow.
    """
    continuation = block[0].index("(") + 1
    names = [match.group(1) for line in block if (match := PARAMETER.search(line))]
    if not names:
        return block
    width = max(len(name) for name in names)
    aligned = []
    for index, line in enumerate(block):
        body = line if index == 0 else " " * continuation + line.lstrip()
        aligned.append(PARAMETER.sub(lambda m: f"{m.group(1):<{width}} : ", body, count=1).rstrip())
    return aligned


def format_text(text: str) -> str:
    """The file with every member run and every wrapped signature aligned."""
    lines = [one_space_before_attributes(line) for line in separate_documented_members(text.splitlines())]
    out: list[str] = []
    run: list[dict[str, str]] = []

    def flush() -> None:
        out.extend(align_run(run) if run else [])
        run.clear()

    index = 0
    while index < len(lines):
        line = lines[index]
        if opens_a_signature(line):
            flush()
            block = [line]
            while index + 1 < len(lines) and not code_of(block[-1]).endswith(";"):
                index += 1
                block.append(lines[index])
            out.extend(align_signature(block))
            index += 1
            continue
        parts = split_member(line) if line.startswith((" ", "\t")) else None
        if parts is None:
            flush()
            out.append(line)
        else:
            run.append(parts)
        index += 1
    flush()
    return "\n".join(out) + ("\n" if text.endswith("\n") else "")


def main() -> int:
    """Rewrites each named file, and reports the ones that changed."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=pathlib.Path)
    args = parser.parse_args()

    changed = 0
    for path in args.paths:
        before = path.read_text(encoding="utf-8")
        after = format_text(before)
        if after != before:
            path.write_text(after, encoding="utf-8")
            changed += 1
            print(f"{path}: aligned", file=sys.stderr)
    return 1 if changed else 0


if __name__ == "__main__":
    sys.exit(main())
