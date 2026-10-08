# === render_test_document.py ==========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Renders the ctest JUnit report as a typst document.

Enumerates every test with its result, what it checks and the requirements it
names. Test names are written as typst string data, so a name holding #, $ or a
bracket cannot change the typesetting.

Writes the .typ only. The caller compiles it, so a runner without typst still
produces a document a later step can typeset.
"""

from __future__ import annotations

import hashlib
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import NamedTuple
from xml.etree import ElementTree

from read_annotations import Annotation, scan
from read_registrations import Registrations, scan_ctest
from read_registrations import scan as scan_registrations
from test_report import Case, EmptyReport, read_cases

# A failing ctest case carries the test's whole output, cut here to keep the
# document readable.
DETAIL_LIMIT = 1500
DETAIL_WIDTH = 110


def typst_string(value: str) -> str:
    """Quotes a value as a typst string literal."""
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    escaped = escaped.replace("\n", "\\n").replace("\r", "").replace("\t", "  ")
    return f'"{escaped}"'


def shorten(detail: str) -> str:
    """Cuts and hard-wraps a failure's output so it fits the page."""
    if len(detail) > DETAIL_LIMIT:
        detail = detail[:DETAIL_LIMIT] + f"\n[... cut at {DETAIL_LIMIT} characters]"

    wrapped = []
    for line in detail.splitlines():
        remainder = line
        while len(remainder) > DETAIL_WIDTH:
            wrapped.append(remainder[:DETAIL_WIDTH])
            remainder = remainder[DETAIL_WIDTH:]
        wrapped.append(remainder)
    return "\n".join(wrapped)


def split_name(classname: str, name: str) -> tuple[str, str, str]:
    """Returns the suite, the case, and the full name as the runner wrote it.

    ctest repeats the whole name in both attributes; gtest's own writer splits them. Where the
    two differ the split is real. Otherwise the suite is the text before the first dot, and a
    name without one has no suite.
    """
    if classname and classname != name:
        return classname, name, f"{classname}.{name}"

    suite, dot, case = name.partition(".")
    if dot:
        return suite, case, name

    return "", name, name


def annotation_for(annotations: dict[str, Annotation], suite: str, name: str) -> Annotation | None:
    """Finds the source annotation for a reported case.

    A parameterised case is reported as Instantiation/Suite.Case/0 and a typed one as
    Suite.Case<T>, so the runtime name is reduced to the one the source declares.
    """
    exact = annotations.get(f"{suite}.{name}")
    if exact is not None:
        return exact

    bare = name.split("<", 1)[0]
    typed = annotations.get(f"{suite}.{bare}")
    if typed is not None:
        return typed

    return annotations.get(f"{suite.rsplit('/', 1)[-1]}.{bare.split('/', 1)[0]}")


# vitest and bats report the spec file as the classname and a sentence as the name. These
# suffixes are what separates one of their cases from a gtest case.
SPEC_SUFFIXES = (
    ".test.ts",
    ".test.tsx",
    ".test.js",
    ".test.jsx",
    ".test.mjs",
    ".spec.ts",
    ".spec.tsx",
    ".spec.js",
    ".spec.jsx",
    ".bats",
)

# A spec name is a sentence, so it gets more room than an identifier.
SPEC_NAME_LIMIT = 170


def is_spec(suite: str) -> bool:
    """Whether the reported suite is a spec file rather than a test suite."""
    return suite.endswith(SPEC_SUFFIXES)


ZERO_WIDTH_SPACE = "\u200b"

# gtest appends its own rendering of a parameter it cannot print, which can run to hundreds of
# characters. The full name stays in the report XML.
PARAMETER_DUMP = "  # GetParam()"
NAME_LIMIT = 90
# A matrix row is one line, so the name is cut to what the column holds and the whole name
# is in the description the row links to. The configuration columns take a fixed width out of
# the 17cm text block and the name takes the rest, so the cut depends on how many there are:
# a name wider than its column prints over the first configuration's cell.
MATRIX_TEXT_WIDTH_CM = 17.0
MATRIX_INSET_CM = 0.282
# One character of the 7.5pt monospace the names are set in, measured off a rendered page.
MATRIX_CHARACTER_CM = 0.149
MATRIX_WIDE_COLUMN_CM, MATRIX_NARROW_COLUMN_CM = 1.75, 1.3
MATRIX_WIDE_UNTIL = 5
# What five configurations allowed. A longer name buys nothing: the whole one is in the
# description the row links to.
MATRIX_NAME_LIMIT = 44
MATRIX_NAME_FLOOR = 24


def matrix_name_space(configurations: int, column: float) -> float:
    """What the 17cm text block leaves the name column, in centimetres."""
    return MATRIX_TEXT_WIDTH_CM - configurations * column - (configurations + 1) * MATRIX_INSET_CM


def matrix_shape(configurations: int) -> tuple[float, int]:
    """The width of a configuration column and the name limit that fits beside it."""
    column = MATRIX_WIDE_COLUMN_CM if configurations <= MATRIX_WIDE_UNTIL else MATRIX_NARROW_COLUMN_CM
    spare = matrix_name_space(configurations, column)
    return column, max(MATRIX_NAME_FLOOR, min(MATRIX_NAME_LIMIT, int(spare / MATRIX_CHARACTER_CM)))


def matrix_fits(configurations: int) -> bool:
    """Whether a name of the floor length still fits beside this many configurations.

    Past the capacity the floor wins over the arithmetic and names print across the first
    configuration's cell. typst reports nothing for that, so the count is refused here instead.
    """
    column, limit = matrix_shape(configurations)
    return limit * MATRIX_CHARACTER_CM <= matrix_name_space(configurations, column)


def matrix_capacity() -> int:
    """The most configurations the results matrix holds."""
    return max(count for count in range(1, 32) if matrix_fits(count))


def display_name(name: str, limit: int = NAME_LIMIT) -> str:
    """The name as the document shows it: the identifier, without gtest's parameter dump."""
    cut = name.split(PARAMETER_DUMP, 1)[0].rstrip()
    if len(cut) > limit:
        return cut[:limit] + "..."
    return cut


def elide(name: str, limit: int) -> str:
    """A name cut to fit, keeping both ends.

    A typed test differs from its siblings only in the parameter at the end, so cutting the tail
    leaves rows that read identically: 294 of VectorTestTemplate's 296 rows shared a displayed
    name. Taking the middle out keeps what identifies the test and what identifies the instance.
    """
    cut = name.split(PARAMETER_DUMP, 1)[0].rstrip()
    if len(cut) <= limit:
        return cut
    head = (limit - 1) * 5 // 9
    return cut[:head] + "\u2026" + cut[len(cut) - (limit - 1 - head) :]


def breakable(name: str) -> str:
    """Puts zero-width spaces into a test name so a long one wraps.

    A test name is one token, and typst will not break inside it.
    """
    out = []
    for index, character in enumerate(name):
        previous = name[index - 1] if index else ""
        if index and (character.isupper() and previous.islower() or character in "_.<"):
            out.append(ZERO_WIDTH_SPACE)
        out.append(character)
    return "".join(out)


AREA_DEPTH = 2


def area_of(declared_in: str) -> str:
    """The part of Sen a test belongs to, from where it is declared.

    The first two path segments, which is the fold the coverage tree uses: both
    libs/core/test/lang/stl_resolver_test.cpp and libs/core/src/lang/resolver.cpp are libs/core.
    """
    parts = [part for part in declared_in.split("/") if part]
    return "/".join(parts[:AREA_DEPTH]) if len(parts) >= AREA_DEPTH else ""


def placement(
    case: Case,
    annotation: Annotation | None,
    registrations: Registrations | None,
) -> tuple[str, str, str]:
    """Where a case belongs: its section, what kind of declaration it is, and its area.

    The document groups by this and the summary counts what it could not place, so both use
    this one function.
    """
    suite, name, _ = split_name(case.classname, case.name)

    if suite and is_spec(suite):
        # The ctest entry in front of the path says which package the spec came from.
        entry = suite.split("/", 1)[0]
        declared_in = registrations.directory_of(entry) if registrations is not None else ""
        return suite, "spec", area_of(declared_in)

    if suite:
        return suite, "suite", area_of(annotation.source if annotation else "")

    directory = registrations.directory_of(name) if registrations is not None else ""
    section, kind = (directory, "origin") if directory else ("", "unplaced")
    return section, kind, area_of(directory)


def case_data(
    cases: list[Case],
    annotations: dict[str, Annotation],
    registrations: Registrations | None = None,
    descriptions: dict[str, str] | None = None,
    per_leg: list[tuple[str, dict[str, Case]]] | None = None,
) -> str:
    """The cases as a typst array of dictionaries.

    With per_leg the entry also carries what each configuration did with the case, in the order
    the configurations are listed. A configuration that never registered the case says so: the
    document has four outcomes and not three, because an absence that renders like a pass is the
    failure this table exists to avoid.
    """
    _, name_limit = matrix_shape(len(per_leg) if per_leg else 0)
    rows = []
    for number, case in enumerate(cases):
        suite, name, full = split_name(case.classname, case.name)
        annotation = annotation_for(annotations, suite, name)

        section, kind, area = placement(case, annotation, registrations)

        limit = SPEC_NAME_LIMIT if kind == "spec" else NAME_LIMIT
        # A test macro carries its own annotation; one registered in CMake carries what the
        # CMake helper was given.
        describes = annotation.description if annotation else (descriptions or {}).get(name, "")
        per = ""
        if per_leg:
            chips = []
            for label, by_name in per_leg:
                ran = by_name.get(match_key(full))
                if ran is None:
                    chips.append(f'(label: {typst_string(label)}, status: "absent", seconds: 0.0)')
                else:
                    chips.append(
                        f"(label: {typst_string(label)}, status: {typst_string(ran.status)},"
                        f" seconds: {round(ran.seconds, 3)})"
                    )
            per = "(" + ", ".join(chips) + ",)"
        fields = ", ".join(
            (
                f"suite: {typst_string(suite)}",
                f"area: {typst_string(area)}",
                f"section: {typst_string(section)}",
                f"kind: {typst_string(kind)}",
                f"name: {typst_string(breakable(display_name(name, limit)))}",
                f"full: {typst_string(full)}",
                f"status: {typst_string(case.status)}",
                f"seconds: {round(case.seconds, 3)}",
                f"detail: {typst_string(shorten(case.detail))}",
                f"does: {typst_string(describes)}",
                f"requires: {typst_string(', '.join(annotation.requirements) if annotation else '')}",
            )
            # Only a combined document carries these three: the matrix is where they are read.
            + (
                (
                    f"per: {per}",
                    f"id: {number}",
                    f"short: {typst_string(elide(name, name_limit))}",
                )
                if per
                else ()
            )
        )
        rows.append(f"  ({fields}),")

    return "#let cases = (\n" + "\n".join(rows) + "\n)\n"


STOP_FLAG = "--stopped-at-first-failure"
COVERAGE_FLAG = "--coverage="
DESCRIPTIONS_FLAG = "--descriptions="
ENVIRONMENT_FLAG = "--environment="
BUILD_FLAG = "--build="
LOGO_FLAG = "--logo="
ORGANIZATION_FLAG = "--organization="
REF_FLAG = "--ref="
RUN_FLAG = "--run="
LEGS_FLAG = "--legs="
APPROVER_FLAG = "--approver="
LOGO_NAME = "sen-logo.svg"


def place_logo(source: Path | None, beside: Path) -> str:
    """Copies the mark beside the document and returns the typst that draws it.

    typst resolves an image path relative to the document, which is written into the build tree
    while the mark lives in the sources.
    """
    if source is None:
        return ""

    if not source.is_file():
        print(f"no logo at {source}; the cover carries the title alone")
        return ""

    shutil.copyfile(source, beside.parent / LOGO_NAME)
    return 'image("' + LOGO_NAME + '", height: 3.6cm)'


# A release is tagged M.m.p and a candidate M.m.p-rcN, which is the pair of patterns
# build_release.yaml fires on.
def run_started(path: Path) -> str:
    """When the run itself began, which is not when this document was written.

    ctest writes one <testsuite> and the merge wraps the suites in a <testsuites> that carries no
    timestamp of its own, so reading only the root put "not recorded" on the cover of every
    document rendered from a merged report, which is all of them.
    """
    try:
        root = ElementTree.parse(path).getroot()
    except ElementTree.ParseError:
        return ""
    if root.get("timestamp"):
        return root.get("timestamp", "")
    suite = root.find("testsuite")
    return suite.get("timestamp", "") if suite is not None else ""


def run_span(legs: list[Leg]) -> str:
    """When the run began and ended, across every configuration in it.

    One configuration's start time is not the run's: on a release the configurations are spread
    over the better part of an hour, and the cover claimed the earliest as though it were all.
    """
    stamps: list[str] = []
    for leg in legs:
        try:
            root = ElementTree.parse(leg.report).getroot()
        except ElementTree.ParseError:
            continue
        stamps.extend(element.get("timestamp", "") for element in root.iter("testsuite"))
        if root.get("timestamp"):
            stamps.append(root.get("timestamp", ""))
    stamps = [stamp for stamp in stamps if stamp]
    if not stamps:
        return ""
    first, last = min(stamps), max(stamps)
    if first == last:
        return first
    # The same day in all but a pathological case, so the date is not worth repeating.
    tail = last.split("T", 1)[1] if last.startswith(first.split("T", 1)[0]) else last
    return f"{first} to {tail}"


def git(checkout: Path, *arguments: str) -> str:
    """Runs git in a checkout and returns its output, empty when it fails or is not installed.

    Every caller here has an answer for not knowing, so a tree git cannot read degrades to that
    instead of ending the render.
    """
    try:
        result = subprocess.run(
            ["git", "--git-dir", str(checkout / ".git"), "--work-tree", str(checkout), *arguments],
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError:
        return ""
    return result.stdout.strip() if result.returncode == 0 else ""


def read_environment(path: Path) -> list[tuple[str, str]]:
    """The machine and toolchain record, as record_environment.py wrote it."""
    recorded: list[tuple[str, str]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        name, tab, value = line.partition("\t")
        if tab and name.strip():
            recorded.append((name.strip(), value.strip()))
    return recorded


# The heading and its lead sit here rather than in the template because a document covering
# several configurations says something different: see environments_of.
ONE_ENVIRONMENT_LEAD = """== Test environment

#block(below: 1.3em, text(size: 8.5pt, fill: SLATE)[
  The item under test is Sen at the code revision above, built in the configuration above and run
  on the machine below. Figures from one configuration do not carry to another, and a duration
  means nothing without the processors it had; a second configuration is a second run of this
  report. What follows was read from the machine by the run itself, not stated by hand.
])

"""


def environment_section(recorded: list[tuple[str, str]]) -> str:
    """The environment as a typst table, or a sentence saying none was recorded."""
    if not recorded:
        return ONE_ENVIRONMENT_LEAD + (
            "#block(below: 1.4em, text(size: 8.5pt, fill: SLATE)[\n"
            "  The run did not record the machine it happened on, so this report cannot state it.\n"
            "])\n"
        )

    rows = ",\n".join(f"  ({typst_string(name)}, {typst_string(value)})" for name, value in recorded)
    return (
        ONE_ENVIRONMENT_LEAD
        + f"""#let environment = (
{rows}
)

#block(below: 1.1em)[
  #grid(
    columns: (auto, 1fr),
    row-gutter: 3.5pt,
    column-gutter: 14pt,
    ..environment.map(((name, value)) => (
      label-text(name), text(font: MONO, size: 8pt, fill: SLATE)[#value]
    )).flatten()
  )
]
"""
    )


# The scripts that decide what the document says. A digest of them tells a reader comparing two
# reports whether the generator changed between them.
GENERATOR_SOURCES = ("render_test_document.py", "read_annotations.py", "read_registrations.py")


def generator_digest() -> str:
    """A short hash over the generator's own sources."""
    digest = hashlib.sha256()
    here = Path(__file__).resolve().parent
    for name in GENERATOR_SOURCES:
        source = here / name
        digest.update(source.read_bytes() if source.is_file() else name.encode())
    return digest.hexdigest()[:12]


def read_descriptions(path: Path) -> dict[str, str]:
    """Reads the descriptions the build collected for tests registered in CMake.

    One tab-separated line per test, written by record_test_description in
    cmake/util/test.cmake. A test declared by a test macro carries its own annotation and is
    not in here.
    """
    descriptions: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        name, tab, description = line.partition("\t")
        if tab and description.strip():
            descriptions[name.strip()] = description.strip()
    return descriptions


FLOOR_FLAG = "--coverage-floor="
DEFAULT_FLOOR = "80"

VALUE_FLAGS = (
    COVERAGE_FLAG,
    FLOOR_FLAG,
    DESCRIPTIONS_FLAG,
    ENVIRONMENT_FLAG,
    BUILD_FLAG,
    LOGO_FLAG,
    ORGANIZATION_FLAG,
    APPROVER_FLAG,
    REF_FLAG,
    RUN_FLAG,
    LEGS_FLAG,
)


def split_arguments(arguments: list[str]) -> tuple[dict[str, str], list[str]]:
    """Splits a command line into its --name=value flags and its positional arguments.

    A misspelled flag is an error. Accepting it as a positional argument would make it the
    report path or the commit, and the document would be rendered from the wrong input.
    """
    flags: dict[str, str] = {}
    positional: list[str] = []
    for argument in arguments:
        if argument == STOP_FLAG:
            continue
        name, separator, value = argument.partition("=")
        if separator and f"{name}=" in VALUE_FLAGS:
            flags[f"{name}="] = value
        elif argument.startswith("-"):
            raise ValueError(f"unknown option {argument}")
        else:
            positional.append(argument)
    return flags, positional


# llvm-cov prints the filename and then three or four triples: regions, functions and lines, each
# as a count, a missed count and a percentage, and a fourth for branches when the build measured
# them. The document does not report the branch figure: llvm-cov counts a source conditional once
# per macro expansion and per template instantiation, so a file built from a macro over a type
# matrix reads far worse than it is, and a reader cannot tell which rows that affects.
COVERAGE_SHAPES = (10, 13)
LINES_TOTAL, LINES_MISSED, LINES_COVER = 7, 8, 9


def percentage(field: str) -> float | None:
    """A percentage cell, or nothing where llvm-cov printed something else.

    A cell that cannot be read is reported as not measured rather than dropping the row, which
    would lose the file's line counts with it.
    """
    try:
        return float(field.rstrip("%"))
    except ValueError:
        return None


class FileCoverage(NamedTuple):
    """One file's row of the report: what was measured and what no test reached."""

    name: str
    lines: int
    lines_missed: int
    lines_cover: float


def shown(value: float | None) -> str:
    """A percentage for the document, or a note that the build did not measure it."""
    return "not measured" if value is None else f"{value:.2f}%"


def read_coverage(path: Path) -> tuple[dict[str, str], list[FileCoverage]]:
    """Reads an llvm-cov report into its total row and a row per file.

    A row counts when it has the column shape above and its line counts are numbers; the
    percentages may be dashes.
    """
    totals: dict[str, str] = {}
    files: list[FileCoverage] = []

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        fields = line.split()
        if len(fields) not in COVERAGE_SHAPES:
            continue

        try:
            count = int(fields[LINES_TOTAL])
            missed = int(fields[LINES_MISSED])
        except ValueError:
            continue

        regions = percentage(fields[3])
        functions = percentage(fields[6])
        lines = percentage(fields[LINES_COVER])

        if fields[0] == "TOTAL":
            totals = {
                "Lines": shown(lines),
                "Functions": shown(functions),
                "Regions": shown(regions),
                "Lines measured": f"{count}",
                "Lines uncovered": f"{missed}",
            }
        else:
            # A file with no percentage of its own still has counts to recover one from.
            if lines is None:
                lines = 100.0 if count == 0 else 100.0 * (count - missed) / count
            files.append(FileCoverage(fields[0], count, missed, lines))

    return totals, files


FOLDER_DEPTH = 2


class FolderCoverage(NamedTuple):
    """One folder of the tree, with the figure the report holds it to."""

    level: int
    label: str
    lines: int
    lines_missed: int
    lines_cover: float


def coverage_tree(files: list[FileCoverage]) -> list[FolderCoverage]:
    """Folds the per-file rows into folders.

    Two levels: the top directory and the package under it.
    """
    totals: dict[str, list[int]] = {}
    for row in files:
        parts = row.name.split("/")
        for depth in range(1, min(FOLDER_DEPTH, len(parts) - 1) + 1):
            key = "/".join(parts[:depth])
            entry = totals.setdefault(key, [0, 0])
            entry[0] += row.lines
            entry[1] += row.lines_missed

    rows: list[FolderCoverage] = []
    for key in sorted(totals):
        count, missed = totals[key]
        if count == 0:
            continue
        rows.append(FolderCoverage(key.count("/"), key, count, missed, 100.0 * (count - missed) / count))
    return rows


def coverage_section(totals: dict[str, str], files: list[FileCoverage], floor: str) -> str:
    """The coverage figures as a typst section."""
    held = float(totals["Lines"].rstrip("%")) >= float(floor)
    verdict = (
        f"At or above the {floor}% line floor, so the build holds."
        if held
        else f"Below the {floor}% line floor, which fails the build."
    )

    measured = int(totals["Lines measured"])
    uncovered = int(totals["Lines uncovered"])
    covered = measured - uncovered

    # The percentage goes beside the text so the table can colour it the way the folder rows below
    # are coloured; a figure that cannot be read as one is left in the ink colour.
    def percentage(value: str) -> float:
        try:
            return float(value.rstrip("%"))
        except ValueError:
            return -1.0

    headline = ", ".join(
        f'("{name}", "{totals[name]}", {percentage(totals[name]):.4f})' for name in ("Lines", "Functions", "Regions")
    )

    folders = ",\n".join(
        f"  ({row.level}, {typst_string(row.label)}, {row.lines}, {row.lines_missed},"
        f" {row.lines_cover:.4f}, {typst_string(f'{row.lines_cover:.1f}%')})"
        for row in coverage_tree(files)
    )

    return f"""== Coverage

#let coverage-headline = ({headline})
#let coverage-folders = (
{folders}
)
// The same table as the one under Summary, for the same reason: a row of large figures reads as
// the point of the section, and the point of this one is the folders below.
#block(above: 0.8em, below: 0.6em)[
  #table(
    columns: (1fr, 1fr, 1fr),
    align: (left, left, left),
    inset: (x: 4pt, y: 4pt),
    stroke: none,
    table.header(..column-labels(coverage-headline.map(((name, value, pct)) => name))),
    table.hline(stroke: 0.6pt + RULE),
    ..coverage-headline.map(((name, value, pct)) => text(
      size: 10.5pt, weight: "bold", fill: if pct < 0 {{ INK }} else {{ cover-colour(pct) }},
    )[#value]),
  )
]

#block(below: 1.4em, text(size: 8.5pt, fill: SLATE)[
  #text(fill: INK, weight: "bold")[#thousands({covered})] of #thousands({measured}) measured lines are
  reached by a test; #thousands({uncovered}) are not. {verdict}
])

#block(above: 0.7em, below: 1em, text(size: 8pt, fill: SLATE)[
  Every measured line, folded into the directory it sits in. A file says where a line is missing; a
  folder says which part of Sen is thin.
])

#table(
  columns: (1fr, 3.6cm, 2.4cm, 2.6cm),
  align: (left, left + horizon, right, right),
  inset: (x: 4pt, y: 3.5pt),
  stroke: none,
  table.header(..column-labels(("Folder", "", "Lines", "Lines covered"))),
  table.hline(stroke: 0.6pt + RULE),
  ..coverage-folders
    .map(((level, label, count, missed, cover, shown)) => (
      block(inset: (left: level * 12pt))[
        #text(font: MONO, size: if level == 0 {{ 8.5pt }} else {{ 8pt }},
              weight: if level == 0 {{ "bold" }} else {{ "regular" }},
              fill: if level == 0 {{ INK }} else {{ SLATE }})[#label]
      ],
      bar(cover / 100.0),
      text(size: 8pt, fill: SLATE)[#thousands(count)],
      text(size: 8pt, weight: if level == 0 {{ "bold" }} else {{ "regular" }},
           fill: cover-colour(cover))[#shown],
    ))
    .flatten()
)

"""


def completeness(cases: list[Case], stopped_at_first_failure: bool) -> str:
    """Whether the enumeration is the whole suite.

    A failure means the run was cut short only when it was asked to stop at the first one, so
    the caller has to say which it was.
    """
    failures = sum(1 for case in cases if case.status == "failed")
    if failures and stopped_at_first_failure:
        return "The run stopped at the first failure, so this enumerates less than the suite."
    return "The whole suite ran."


def conclusion_block(
    cases: list[Case],
    line_coverage: float | None,
    floor: str,
    stopped_at_first_failure: bool,
) -> tuple[str, str]:
    """The verdict box and the table of criteria it was reached by, as two pieces of typst."""
    failures = sum(1 for case in cases if case.status == "failed")
    skipped = sum(1 for case in cases if case.status == "skipped")
    # A test that did not run verified nothing, so it cannot count towards a pass: without this
    # a run that skipped everything has no failures and passes.
    executed = len(cases) - skipped
    held = line_coverage is not None and line_coverage >= float(floor)
    cut_short = bool(failures) and stopped_at_first_failure

    if failures:
        results = f"{tests(failures)} did not pass."
    elif executed == 0:
        results = f"No test executed. {tests(len(cases))} {were(len(cases))} skipped, so nothing was verified."
    elif skipped:
        results = f"Every test that ran passed; {tests(skipped)} skipped and ran nothing."
    else:
        results = "Every test passed."
    met = not failures and executed > 0 and (line_coverage is None or held) and not cut_short

    # A rule and a reading are different things, so they get a column each and the mark says
    # whether the one met the other. A criterion nothing measured is marked neither way.
    outcomes_met = not failures and executed > 0
    complete = not cut_short
    if line_coverage is None:
        coverage_rule = f"≥ {floor}% of lines."
        coverage_seen = "This run measured none, so the floor was not tested."
    else:
        coverage_rule = f"≥ {floor}% of lines."
        coverage_seen = f"{line_coverage:.2f}% of lines."
    rows = [
        ("Outcomes", "Every test passes.", results, outcomes_met),
        ("Coverage", coverage_rule, coverage_seen, held if line_coverage is not None else None),
        ("Completeness", "All the tests are executed.", completeness(cases, stopped_at_first_failure), complete),
    ]
    body = "\n".join(
        f"    mark({'none' if ok is None else str(ok).lower()}),"
        f' text(size: 8.5pt, weight: "bold")[{name}],'
        f" text(size: 8.5pt, fill: SLATE)[{rule}],"
        f" text(size: 8.5pt, fill: SLATE)[{seen}],"
        for name, rule, seen, ok in rows
    )
    word = "PASSED" if met else "FAILED"
    colour = "RESULT-COLOUR.passed" if met else "RESULT-COLOUR.failed"

    verdict = f"""#block(above: 0.9em, below: 1.1em)[
  #verdict-badge("{word}", {colour})
]
"""
    criteria = f"""#block(above: 1.2em, below: 0.5em, text(size: 8.5pt, fill: SLATE)[
  Every criterion below has to hold for the run to read PASSED.
])

// The mark carries the verdict for one criterion; a criterion nothing measured gets a dash,
// which is not the same as one that was tested and held.
#let mark(ok) = if ok == none {{
  text(font: MONO, size: 9pt, fill: SLATE)[#sym.dash.en]
}} else if ok {{
  text(font: MONO, size: 9pt, weight: "bold", fill: RESULT-COLOUR.passed)[#sym.checkmark]
}} else {{
  text(font: MONO, size: 9pt, weight: "bold", fill: RESULT-COLOUR.failed)[#sym.crossmark]
}}

#block(below: 1.4em)[
  #table(
    columns: (0.8cm, 2.9cm, 1fr, 1fr),
    align: (center + horizon, left, left, left),
    inset: (x: 4pt, y: 3.5pt),
    stroke: none,
    table.header(..column-labels(("", "Criterion", "What it requires", "What this run did"))),
    table.hline(stroke: 0.6pt + RULE),
{body}
  )
]
"""
    return verdict, criteria


def deviations_section(
    cases: list[Case],
    unresolved: int,
    skipped: int | None = None,
    unmeasured: list[str] | None = None,
    untraced: int | None = None,
) -> str:
    """What the run did not account for, which the totals do not show.

    A document covering several configurations passes the count itself: its cases carry the
    worst outcome across configurations, where skipped means every one of them skipped it, and
    a reader looking for skipped tests means any one of them.
    """
    across = skipped is not None
    if skipped is None:
        skipped = sum(1 for case in cases if case.status == "skipped")

    items = []
    if skipped:
        where = "on at least one configuration" if across else "in this run"
        items.append(f"{tests(skipped)} {were(skipped)} skipped {where}, and so verified nothing.")
    if untraced:
        items.append(
            f"{tests(untraced)} name no requirement, so this report cannot say what they trace to."
            " An identifier is written beside the test that carries one; there is no register here"
            " to check the rest against."
        )
    if unresolved:
        items.append(
            "Registered under a name built from CMake variables, which cannot be traced to a"
            f" directory without evaluating the build: {tests(unresolved)}, listed last and outside"
            " any area."
        )
    # Named from the areas that actually carry no figure rather than from a list kept by hand,
    # which had drifted: it named two components that no longer exist and omitted every area
    # coverage never reaches.
    if unmeasured:
        items.append(
            "Areas that carry tests but no coverage figure: "
            + ", ".join(unmeasured)
            + ". The measurement reads compiled C++ only, so areas tested through TypeScript,"
            " shell or fixtures carry none, as do the generated sources and the deprecated"
            " components."
        )
    else:
        items.append(
            "Where an area carries no coverage figure, the measurement did not reach it: it reads"
            " compiled C++ only, and the generated sources and deprecated components are outside it."
        )

    listed = "\n".join(f"  - {item}" for item in items)
    return f"""== Deviations and exclusions

#block(below: 1.4em, text(size: 8.5pt, fill: SLATE)[
{listed}
])
"""


def tests(count: int) -> str:
    """A count of tests that reads correctly when there is one of them."""
    return f"{count} test" if count == 1 else f"{count} tests"


def were(count: int) -> str:
    """The verb that follows a count of tests."""
    return "was" if count == 1 else "were"


ISSUER = "Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS"
STANDARD = "ISO/IEC/IEEE 29119-3:2013"


def prose(heading: str, *paragraphs: str) -> str:
    """A section of plain prose, which most clause 6.4 items are."""
    body = "\n\n".join(f"  {paragraph}" for paragraph in paragraphs)
    return f"""{heading}

#block(below: 1.4em, text(size: 8.5pt, fill: SLATE)[
{body}
])
"""


def listing(heading: str, lead: str, items: list[str]) -> str:
    """A section whose body is a list, with a sentence introducing it."""
    listed = "\n".join(f"  - {item}" for item in items)
    return f"""{heading}

#block(below: 1.4em, text(size: 8.5pt, fill: SLATE)[
  {lead}

{listed}
])
"""


def document_information(organization: str, approver: str, commit: str) -> str:
    """Clause 6.4.2: who issued the document, who approved it, and how it changes."""
    items = [
        "*Identification.* This document is identified by its title together with the code"
        f" revision ({commit}) and the generation timestamp on the cover. Those three name"
        " one document and no other.",
        f"*Issuing organization.* {organization}.",
    ]
    # Written only when someone gave an approval. The conformance table reports the item
    # unclaimed when nobody did.
    if approver:
        items.append(f"*Approval authority.* Approved by {approver}.")
    items.append(
        "*Change history.* None. Each run writes a new document rather than revising an"
        " earlier one, so there is no chain of versions to record; the identity on the cover"
        " says which run produced this one."
    )
    return prose("= Document information", *items)


def references(report: Path, coverage: Path | None, dependencies: str, legs: int = 1) -> str:
    """Clause 6.4.3.2: what this document rests on, and where to find it."""
    if legs == 1:
        enumerated = f"The test report this document enumerates: `{report.name}`, written by ctest during the run."
    else:
        enumerated = (
            f"The {legs} test reports this document enumerates, one per configuration, each written"
            " by ctest during its own run and each named with its digest under Test deliverables."
        )
    items = [enumerated]
    if coverage is not None:
        items.append(f"The coverage report: `{coverage.name}`, written by llvm-cov over the same run.")
    items.append(
        "The Sen repository at the code revision on the cover, which holds the tests, the"
        " descriptions they carry and the generator that produced this document."
    )
    items.append(f"The pinned dependency set: {dependencies}.")
    items.append(
        f"{STANDARD}, Software and systems engineering — Software testing — Part 3: Test"
        " documentation, whose Test Completion Report this document is structured to."
    )
    return listing("= References", "This report rests on the following, and on nothing else:", items)


def glossary() -> str:
    """Clause 6.4.3.3: the words this document uses in a particular way."""
    terms = [
        "*Area.* The part of Sen a test belongs to, taken as the first two path segments of where"
        " it is declared: `libs/core`, `components/term`. The same fold the coverage figures use.",
        "*Suite.* A group of C++ cases sharing a gtest fixture; also used here for the spec file"
        " or registering directory that groups a test declared outside C++.",
        "*Case.* One test, as the runner reports it. A parameterised C++ test contributes one case per parameter.",
        "*Covered line.* A line of hand-written Sen source that at least one test executed, as"
        " llvm-cov counts it. Generated sources and deprecated components are excluded.",
        "*Skipped.* A test the run chose not to execute. It verifies nothing in this run.",
        "*Requirement identifier.* A `SEN-` or `REQ-` reference carried in the test's own source,"
        " naming the requirement the test was written for.",
        "*ctest, gtest, vitest, bats.* The runners: ctest schedules every test; gtest declares the"
        " C++ cases; vitest the TypeScript ones; bats the installer's shell cases.",
    ]
    return listing("= Glossary", "Terms this document uses in a particular sense:", terms)


def residual_risks(
    cases: list[Case],
    line_coverage: float | None,
    floor: str,
    area_coverage: dict[str, float],
    area_missed: dict[str, int],
) -> str:
    """Clause 6.4.4.6: the risks the run itself measured.

    A test that failed, a test that did not run, a line no test reached. Risks a run cannot
    observe are named as unlisted instead of being left out silently.
    """
    failures = sum(1 for case in cases if case.status == "failed")
    skipped = sum(1 for case in cases if case.status == "skipped")

    items = []
    if failures:
        items.append(
            f"{tests(failures)} failed and are reported unfixed. What they guard is unverified in"
            " this run; each is listed under Failures with its output."
        )
    if skipped:
        items.append(
            f"{tests(skipped)} were skipped. What they guard is unverified in this run, and a"
            " skipped test reports nothing about whether it would pass."
        )
    if line_coverage is not None:
        # Ranked by lines nobody reached, not by percentage. A small area at 33% holds tens of
        # lines while a large one at 80% holds thousands, so ranking by percentage names the
        # areas that matter least and omits the ones that carry the risk.
        worst = sorted(
            (area for area in area_missed if "/" in area and area_missed[area]),
            key=lambda area: area_missed[area],
            reverse=True,
        )[:5]
        below = sorted(
            (area for area, value in area_coverage.items() if value < float(floor) and "/" in area),
            key=lambda area: area_coverage[area],
        )
        items.append(
            f"{100.0 - line_coverage:.2f}% of measured lines are reached by no test. A defect on"
            " one of them cannot be found by this suite."
        )
        if worst:
            items.append(
                "Where most of those lines are: "
                + "; ".join(f"{area_missed[area]} in {area}, at {area_coverage[area]:.1f}%" for area in worst)
                + "."
            )
        if below:
            items.append(
                f"Areas under the {floor}% floor: "
                + ", ".join(f"{area} at {area_coverage[area]:.1f}%" for area in below)
                + "."
            )
    items.append(
        "The deprecated components and the generated sources are outside the measurement"
        " entirely, so no figure here says anything about them."
    )
    items.append(
        "A test run cannot observe every risk. Design faults the suite does not probe, and"
        " behaviour under conditions nobody wrote a test for, are not listed here, and their"
        " absence from this list is not evidence that there are none."
    )
    return listing(
        "== Residual risks",
        "Untreated at the end of this run, each one measured by it:",
        items,
    )


def file_digest(path: Path) -> str:
    """A short sha256 over one input, in the same form the generator uses for its own sources.

    Twelve characters, because that is what the cover already carries for the generator and a
    reader comparing two documents needs one form, not two.
    """
    try:
        return hashlib.sha256(path.read_bytes()).hexdigest()[:12]
    except OSError:
        return "not read"


def deliverables(report: Path, coverage: Path | None, descriptions: Path | None, environment: Path | None) -> str:
    """Clause 6.4.4.7: what the test effort produced and where it is.

    Each input the document took data from carries the digest of the file as it was read. The
    document asserts figures it did not measure itself, and without this a reader has no way to
    tell whether the file beside it is the one those figures came from.
    """
    items = [f"`{report.name}` — the test report, one entry per case, in JUnit XML. sha256 `{file_digest(report)}`."]
    if coverage is not None:
        items.append(
            f"`{coverage.name}` and the lcov and HTML forms beside it — the coverage measurement."
            f" sha256 `{file_digest(coverage)}`."
        )
    if descriptions is not None:
        items.append(
            f"`{descriptions.name}` — what each test registered in CMake checks. sha256 `{file_digest(descriptions)}`."
        )
    if environment is not None:
        items.append(
            f"`{environment.name}` — the machine and toolchain the run happened on."
            f" sha256 `{file_digest(environment)}`."
        )
    else:
        items.append("`test-environment.tsv` — the machine and toolchain the run happened on.")
    return listing(
        "== Test deliverables",
        "Produced by the run, all in the build directory named on the cover. A digest is the"
        " first twelve characters of the sha256 of the file as this document read it:",
        items,
    )


def combined_deliverables(legs: list[Leg]) -> str:
    """Clause 6.4.4.7, for a document covering several configurations.

    Every configuration writes files under the same names, so the name alone does not identify
    one: the configuration and the digest together do. A figure in this document can be traced to
    the file it was read from, and a file can be checked against what is listed here.
    """
    items = []
    for leg in legs:
        files = [("the test report, one entry per case, in JUnit XML", leg.report)]
        if leg.measured is not None:
            files.append(("the coverage measurement", leg.measured))
        if leg.descriptions_file is not None:
            files.append(("what each test registered in CMake checks", leg.descriptions_file))
        if leg.environment_file is not None:
            files.append(("the machine and toolchain it ran on", leg.environment_file))
        named = "; ".join(f"`{path.name}` ({what}) sha256 `{file_digest(path)}`" for what, path in files)
        items.append(f"*{leg.configuration}* — {named}.")
    lead = (
        "Produced by the run, one set per configuration. Every configuration writes these files"
        " under the same names, so a name alone does not say which build it came from; the"
        " configuration and the digest below do. A digest is the first twelve characters of the"
        " sha256 of the file as this document read it. A configuration that measured no"
        " coverage lists none:"
    )
    return listing("== Test deliverables", lead, items)


def reusable_assets() -> str:
    """Clause 6.4.4.8: what outlives the run."""
    items = [
        "The test sources themselves, in the repository beside the code they check, each carrying its own description.",
        "The configuration fixtures under `examples/config` and `test/util`, which the smoke tests start and stop.",
        "The recorded archive under `components/recorder/test/data`, reused by the archive CLI and"
        " the database binding tests rather than regenerated per run.",
        "The generator in `.github/scripts`, which produces this document from any run's output.",
    ]
    return listing(
        "== Reusable test assets",
        "Held in the repository and reused by later runs rather than rebuilt:",
        items,
    )


def blockers() -> str:
    """Clause 6.4.4.4, which an automated run answers by saying what it does not observe."""
    return prose(
        "== Factors that blocked progress",
        "None are recorded. The run observes test outcomes and nothing else, so an unavailable"
        " environment or a dependency that would not build is invisible to it. A test that could"
        " not run at all appears as skipped, and is counted under Deviations and exclusions.",
    )


def lessons() -> str:
    """Clause 6.4.4.9, which an automated run also has no answer for."""
    return prose(
        "== Lessons learned",
        "None are recorded.",
    )


def conformance(approver: str, combined: bool = False) -> str:
    """Where each clause 6.4 item is answered, so the claim can be checked.

    A document covering several configurations renames and moves three of the sections
    these point at, and a pointer that cannot be followed defeats the table.
    """
    environments = "Test environments, and Summary" if combined else "Summary, and the test environment under it"
    measures = "Coverage" if combined else "Coverage, and Tests by area"
    enumeration = "Test results and Test descriptions" if combined else "The tests"
    criteria_at = "The criteria opening Acceptance" if combined else "The criteria under Summary"
    rows = [
        ("6.4.2.2", "Unique identification", "Cover, and Document information"),
        ("6.4.2.3", "Issuing organization", "Document information"),
        ("6.4.2.4", "Approval authority", "Document information" if approver else "Not claimed"),
        ("6.4.2.5", "Change history", "Document information"),
        ("6.4.3.1", "Scope", "Purpose and scope"),
        ("6.4.3.2", "References", "References"),
        ("6.4.3.3", "Glossary", "Glossary"),
        ("6.4.4.1", "Summary of testing performed", environments),
        ("6.4.4.2", "Deviations from planned testing", "Deviations and exclusions"),
        ("6.4.4.3", "Test completion evaluation", criteria_at),
        ("6.4.4.4", "Factors that blocked progress", "Factors that blocked progress"),
        ("6.4.4.5", "Test measures", measures),
        ("6.4.4.6", "Residual risks", "Residual risks"),
        ("6.4.4.7", "Test deliverables", "Test deliverables"),
        ("6.4.4.8", "Reusable test assets", "Reusable test assets"),
        ("6.4.4.9", "Lessons learned", "Lessons learned"),
    ]
    body = "\n".join(f"  raw({typst_string(clause)}), [{name}], [{where}]," for clause, name, where in rows)
    # Said in the text as well as the table: an approval is the one item a person has to give.
    unclaimed = (
        ""
        if approver
        else (
            " Approval authority is not claimed: this document is issued by the run that wrote"
            " it, and a reader who needs an approved record should treat it as the input to one."
        )
    )
    return f"""= Conformance

#block(below: 1.2em, text(size: 8.5pt, fill: SLATE)[
  This document is structured to the Test Completion Report of {STANDARD}, clause 6.4. Each item
  that clause names is answered below, so the claim can be checked. Two of them, factors that
  blocked progress and lessons learned, are answered by saying that an automated run records
  neither.{unclaimed}

  The per-test enumeration under {enumeration} is not part of clause 6.4, which asks for a
  summary. It is included because it is the part readers most often want, and it belongs to
  the dynamic test process documents of the same standard.
])

#table(
  columns: (2.2cm, 1fr, 1fr),
  align: (left, left, left),
  inset: (x: 4pt, y: 3.5pt),
  stroke: none,
  table.header(..column-labels(("Clause", "Item", "Answered in"))),
  table.hline(stroke: 0.6pt + RULE),
{body}
)
"""


def render(
    cases: list[Case],
    meta: list[tuple[str, str]],
    *,
    annotations: dict[str, Annotation] | None = None,
    registrations: Registrations | None = None,
    logo: str = "",
    coverage: str = "",
    descriptions: dict[str, str] | None = None,
    area_coverage: dict[str, float] | None = None,
    line_coverage: float | None = None,
    verdict: str = "",
    criteria: str = "",
    acceptance: str = "",
    by_area_section: str = "",
    tests_section: str = "",
    summary_clauses: str = "",
    closing_clauses: str = "",
    headline_totals: str = "",
    headline_label: str = '"Tests"',
    scope_opening: str = "This is the completion report for one run of Sen's test suite.",
    duration_label: str = '"Duration"',
    configuration_names: str = "()",
    environments_section: str = "",
    per_leg: list[tuple[str, dict[str, Case]]] | None = None,
    environment: str = "",
    clause: dict[str, str] | None = None,
) -> str:
    """The whole typst document."""
    # The totals row of the area index reads its figure from here under the empty key, which no
    # area path can be. residual_risks skips it for the same reason: it wants paths.
    areas = sorted((area_coverage or {}).items())
    if line_coverage is not None:
        areas.append(("", line_coverage))
    covered = ",\n".join(
        f"  {typst_string(area)}: (value: {value:.4f}, shown: {typst_string(f'{value:.1f}%')})" for area, value in areas
    )
    parts = clause or {}
    # With no mark the stack holds the name alone.
    mark = f"align(center, {logo}),\n    " if logo else ""
    matrix_column, _ = matrix_shape(len(per_leg) if per_leg else 0)
    meta_rows = "\n".join(f"  ({typst_string(k)}, {typst_string(v)})," for k, v in meta)
    described = annotations or {}

    return f"""// Generated by .github/scripts/render_test_document.py. Do not edit.
//
// The data is generated above the presentation, so the layout can be changed without
// regenerating, and a test name cannot reach the typesetter as markup.

#set document(title: "Sen Software Test Report", author: "Sen")
// The running header names the suite the page is in, so a continuation page says what it holds.
#set page(
  paper: "a4",
  margin: (top: 2.2cm, rest: 2cm),
  numbering: "1 / 1",
  header: context {{
    let here-page = here().page()
    // The nearest heading above, whatever its level.
    let suites = query(heading).filter(h => h.level <= 4 and h.location().page() <= here-page)
    if suites.len() > 0 and here-page > 1 {{
      set text(size: 8pt, fill: rgb("#4a5058"))
      grid(
        columns: (1fr, auto),
        align: (left, right),
        text[Software Test Report],
        suites.last().body,
      )
      v(-6pt)
      line(length: 100%, stroke: 0.4pt + rgb("#dcdfe3"))
    }}
  }},
)
// Named, not a fallback chain: metrics move the page count. These are the faces the build
// image carries; elsewhere typst warns and substitutes.
#let DISPLAY = "Libertinus Serif"
#let BODY = "Liberation Sans"
#let MONO = "DejaVu Sans Mono"

#let INK = rgb("#1a1d21")
#let SLATE = rgb("#4a5058")
#let RULE = rgb("#dcdfe3")
#let TRACK = rgb("#ecedf0")
#let ACCENT = rgb("#2d4a63")
#let RESULT-COLOUR = (passed: rgb("#1f6b4f"), failed: rgb("#9c2b3e"), skipped: rgb("#7b8187"))
#let WARN = rgb("#9a6b1f")

// Figures line up in columns all through this document, so the digits are the same width.
#set text(font: BODY, size: 9pt, fill: INK, number-width: "tabular")
// Paragraph spacing, set wider than the leading inside a paragraph.
#set par(leading: 0.68em, spacing: 1.15em, justify: false)
#show raw: set text(font: MONO)

// Numbered to the depth the contents lists. A suite heading below that keeps its name alone: a
// fourth level of digits against two hundred pages of suites is noise, not a reference.
#let NUMBERED-DEPTH = 3
#set heading(numbering: (..levels) => {{
  if levels.pos().len() <= NUMBERED-DEPTH {{ numbering("1.1", ..levels) }}
}})
// The show rules below build their own heading, so each has to place the number itself.
#let heading-number(it) = if it.numbering != none and it.level <= NUMBERED-DEPTH {{
  [#counter(heading).display(it.numbering)#h(0.6em)]
}}

// Level one is a part of the document, level two a section or an area of Sen, level three a
// suite. A part opens a page; the break is weak, so one already at the top gets no blank page.
// The two front-matter parts are the exception: each is a few paragraphs, and a page apiece left
// three in a row three-quarters empty, so they run on from the document information.
#let runs-on = ([Purpose and scope], [References], [Glossary])
#show heading.where(level: 1): it => {{
  if not runs-on.contains(it.body) {{
    pagebreak(weak: true)
  }}
  block(above: 0.4em, below: 0.95em, width: 100%)[
    #text(font: DISPLAY, size: 17pt, weight: "bold", fill: INK)[#heading-number(it)#it.body]
    #v(5pt, weak: true)
    #line(length: 100%, stroke: 1.8pt + INK)
  ]
}}
#show heading.where(level: 2): it => block(above: 1.6em, below: 0.75em)[
  #text(font: DISPLAY, size: 14pt, weight: "bold", fill: INK)[#heading-number(it)#it.body]
]
#show heading.where(level: 3): it => block(above: 1.5em, below: 0.6em)[
  #text(font: DISPLAY, size: 12pt, weight: "bold", fill: INK)[#heading-number(it)#it.body]
]
// Room above, so the heading does not sit flush against the first test under it.
#show heading.where(level: 4): it => block(above: 2em, below: 0.8em, width: 100%)[
  #text(font: DISPLAY, size: 11pt, weight: "bold", fill: INK)[#it.body]
  #v(3.5pt, weak: true)
  #line(length: 100%, stroke: 0.8pt + ACCENT)
]

#let label-text(name) = text(size: 6.5pt, fill: SLATE, weight: "bold", tracking: 0.09em)[#upper(name)]
#let column-labels(names) = names.map(name => label-text(name))

// Digits in groups of three with a thin space; a comma reads as a decimal point in some locales.
#let thousands(n) = {{
  let digits = str(n).clusters()
  let parts = ()
  let i = digits.len()
  while i > 3 {{
    parts.push(digits.slice(i - 3, i).join(""))
    i -= 3
  }}
  parts.push(digits.slice(0, i).join(""))
  parts.rev().join(sym.space.thin)
}}
// A coloured dot and the word for it.
#let status-mark(status) = {{
  let colour = RESULT-COLOUR.at(status)
  box(baseline: -0.05em, circle(radius: 0.16em, fill: colour, stroke: none))
  h(3.5pt)
  text(fill: colour)[#status]
}}
// The configurations this document covers, in the order their columns appear.
#let configurations = {configuration_names}

#let config-label(name) = text(size: 5.5pt, fill: SLATE, weight: "bold", tracking: 0.01em)[#upper(name)]
// The coverage table holds compiler, architecture and build type on one line in a narrower
// column, so its heading is a half point smaller again.
#let cover-label(name) = text(size: 5pt, fill: SLATE, weight: "bold", tracking: 0em)[#upper(name)]

// One cell of the results matrix: the mark for an outcome. A configuration that never
// registered the case carries a dash, which is not a result of zero.
#let resultCell(p) = if p.status == "absent" {{
  text(size: 7.5pt, weight: "bold", fill: WARN)[#sym.dash.en]
}} else {{
  text(font: MONO, size: 7.5pt, weight: "bold", fill: RESULT-COLOUR.at(p.status))[#(
    if p.status == "passed" {{ sym.checkmark }}
    else if p.status == "failed" {{ sym.crossmark }}
    else {{ sym.circle.small }}
  )]
}}

// A row per test, a column per configuration. The name links to the description below, and a
// document that compiles has no broken link: typst refuses a label that does not resolve.
#let resultsOf(wanted) = block(below: 1.1em)[
  #table(
    columns: (1fr, ..configurations.map(_ => {matrix_column}cm)),
    align: (left + horizon, ..configurations.map(_ => center + horizon)),
    inset: (x: 4pt, y: 2.5pt),
    stroke: none,
    table.header(
      label-text("Test"),
      ..configurations.map(c => align(right)[
        #config-label(c.at(0))#if c.at(1) != "" [#linebreak()#config-label(c.at(1))]
      ]),
    ),
    table.hline(stroke: 0.6pt + RULE),
    ..wanted.map(case => (
      link(label("d" + str(case.id)))[#text(font: MONO, size: 7.5pt, fill: INK)[#case.short]],
      ..case.per.map(p => if p.status == "absent" {{ resultCell(p) }} else {{
        [#resultCell(p)#h(2.5pt)#text(size: 6pt, fill: SLATE)[#calc.round(p.seconds, digits: 2) s]]
      }}),
    )).flatten(),
  )
]

// The description, carrying the label the matrix links to.
#let describedOf(wanted) = {{
  for (index, case) in wanted.enumerate() {{
    if index > 0 {{
      v(0.4em)
      line(length: 100%, stroke: 0.4pt + RULE)
    }}
    block(breakable: false, above: 0.45em, below: 0.3em, width: 100%)[
      #grid(
        columns: (1fr, auto),
        align: (left + bottom, right + bottom),
        column-gutter: 10pt,
        [#text(font: MONO, size: 8.5pt, fill: INK)[#case.name]#label("d" + str(case.id))],
        text(size: 7.5pt, font: MONO, fill: SLATE)[#case.requires],
      )
      #if case.does != "" [
        #block(inset: (left: 14pt, top: 2pt))[#text(size: 8.5pt, fill: rgb("#3a4047"))[#case.does]]
      ]
    ]
  }}
}}

// The run's verdict, which a reader should find before any prose. Bordered rather than filled:
// a solid block of colour on the first page of a formal report reads as a warning sticker.
// A box rather than a block, so the label sits on the same line as the word.
#let verdict-badge(word, colour) = box(
  stroke: 1.2pt + colour, radius: 2pt, inset: (x: 11pt, y: 7pt), baseline: 0.3em,
  text(size: 13pt, weight: "bold", tracking: 1.5pt, fill: colour)[#word],
)
#let cover-colour(p) = if p >= 80 {{ RESULT-COLOUR.passed }} else if p >= 60 {{ WARN }} else {{ RESULT-COLOUR.failed }}
#let bar(ratio) = {{
  let filled = calc.min(calc.max(ratio, 0.0), 1.0)
  box(width: 100%, height: 0.5em, radius: 0.07em, fill: TRACK, align(left + horizon,
    box(width: filled * 100%, height: 100%, radius: 0.07em, fill: cover-colour(filled * 100.0)),
  ))
}}

#let meta = (
{meta_rows}
)

{case_data(cases, described, registrations, descriptions, per_leg)}
#let areaCoverage = (
{covered}
)

#let entriesOf(wanted) = {{
  for (index, case) in wanted.enumerate() {{
    // A rule between entries, not after the last.
    if index > 0 {{
      v(0.45em)
      line(length: 100%, stroke: 0.4pt + RULE)
    }}
    block(breakable: false, above: 0.5em, below: 0.35em, width: 100%)[
      #grid(
        columns: (1fr, auto),
        align: (left + bottom, right + bottom),
        column-gutter: 10pt,
        text(font: MONO, size: 8.5pt, fill: INK)[#case.name],
        text(size: 8pt)[
          #if case.requires != "" [#text(font: MONO, size: 7.5pt, fill: SLATE)[#case.requires] #h(7pt)]
          // Only where there is one configuration: with several, the result belongs to each of
          // them and sits in the strip below, where it says which.
          #if "per" not in case [
            #status-mark(case.status)
            #h(7pt) #text(fill: SLATE)[#calc.round(case.seconds, digits: 2) s]
          ]
        ],
      )
      #if case.does != "" [
        #block(inset: (left: 14pt, top: 3pt))[#text(size: 8.5pt, fill: rgb("#3a4047"))[#case.does]]
      ]
    ]
  }}
}}

// Grouped by where a test is declared, and within that by the suite, spec file or directory it
// sits in: a flat list of several hundred suite names cannot be read for what is tested.
#let areas = cases.map(c => c.area).filter(a => a != "").dedup().sorted()
// An area is two segments; the first is the part of the tree it belongs to.
#let groupOf(a) = a.split("/").at(0)
#let leafOf(a) = a.split("/").slice(1).join("/")
#let groups = areas.map(groupOf).dedup().sorted()
#let areasIn(g) = areas.filter(a => groupOf(a) == g)
#let inArea(a) = cases.filter(c => c.area == a)
#let inGroup(g) = cases.filter(c => c.area != "" and groupOf(c.area) == g)
#let unplaced = cases.filter(c => c.area == "")
#let countIn(list, wanted) = list.filter(c => c.status == wanted).len()

// C++ suites first, then spec files, then what CMake registers, in that order in every area.
#let KIND-RANK = (suite: "1", spec: "2", origin: "3", unplaced: "4")
#let sectionsIn(list) = {{
  let names = list.map(c => c.section).dedup()
  names.sorted(key: s => KIND-RANK.at(list.filter(c => c.section == s).first().kind) + "~" + s)
}}

#let indexRow(key, shown, list, level) = {{
  let failed = countIn(list, "failed")
  let skipped = countIn(list, "skipped")
  let heavy = if level == 0 {{ "bold" }} else {{ "regular" }}
  (
    block(inset: (left: level * 12pt))[
      #text(font: MONO, size: if level == 0 {{ 8.5pt }} else {{ 8pt }}, weight: heavy,
            fill: if level == 0 {{ INK }} else {{ SLATE }})[#shown]
    ],
    text(size: 8pt, weight: heavy)[#thousands(list.len())],
    text(size: 8pt, weight: heavy, fill: RESULT-COLOUR.passed)[#thousands(countIn(list, "passed"))],
    if failed > 0 {{ text(size: 8pt, weight: heavy, fill: RESULT-COLOUR.failed)[#thousands(failed)] }} else {{
      text(size: 8pt, fill: SLATE)[0]
    }},
    if skipped > 0 {{ text(size: 8pt, weight: heavy, fill: RESULT-COLOUR.skipped)[#thousands(skipped)] }} else {{
      text(size: 8pt, fill: SLATE)[0]
    }},
    if key in areaCoverage {{
      text(size: 8pt, weight: heavy, fill: cover-colour(areaCoverage.at(key).value))[#areaCoverage.at(key).shown]
    }} else {{ text(size: 8pt, fill: SLATE)[#sym.dash.en] }},
  )
}}

{headline_totals}

#v(3.2cm)

#align(center)[
  // A stack, not paragraph spacing, which held the two 1.4cm apart whatever it was asked for.
  // The edges pin the name's box to its letters, so the spacing here is the spacing that shows.
  #stack(
    spacing: 0.25cm,
    {mark}align(center, text(font: DISPLAY, size: 34pt, weight: "bold", fill: INK,
      top-edge: "cap-height", bottom-edge: "baseline")[Sen]),
  )
  #v(1.83cm)
  #text(font: DISPLAY, size: 26pt, weight: "bold", fill: INK)[Software Test Report]
  #v(1.6cm)
  #block[
    #grid(
      columns: (auto, auto),
      row-gutter: 4pt,
      column-gutter: 16pt,
      align: (right, left),
      ..meta.map(((name, value)) => (label-text(name), text(font: MONO, size: 8pt, fill: SLATE)[#value])).flatten()
    )
  ]
  #v(1.4cm)
  #text(size: 8pt, fill: SLATE, style: "italic")[
    This document has been automatically generated from the test run named above.
    #linebreak()
    Nothing in it is written or kept by hand, and an edited copy is no longer the record of
    anything.
  ]
]

#pagebreak()

#heading(level: 1, outlined: false, numbering: none)[Contents]

// Three levels: the parts, their sections, and each area of Sen. The suites sit a level below
// and are not listed.
#outline(title: none, depth: 3, indent: 1em)

#pagebreak()

{parts["information"]}
= Purpose and scope

#block(below: 1.3em, text(size: 8.5pt, fill: SLATE)[
  {scope_opening} It identifies what ran, states
  what passed and what did not, gives the coverage the run measured, and enumerates every test
  with what it checks, so a reader can see what was verified as well as how much. It reports;
  it does not decide what should be tested.
])

#block(below: 1.3em, text(size: 8.5pt, fill: SLATE)[
  *Covered.* Every test ctest ran: C++ cases declared with a gtest macro, vitest cases from the
  TypeScript packages, bats cases from the installer suite, and the tests a CMakeLists registers
  directly. Each appears once, under the part of Sen it is declared in.
])

#block(below: 1.4em, text(size: 8.5pt, fill: SLATE)[
  *Outside this report.* Sen is verified by means other than tests, and those are not enumerated
  here: static analysis with clang-tidy, the sanitizer lanes, the documentation build, and the
  pull-request checks that hold the coverage floor. They are outside the scope of this report.
  A requirement identifier shown against a test is the one its author wrote it for, and this
  report does not resolve it: it enumerates what each test checks, not what was required.
])

{parts["references"]}
{parts["glossary"]}
{environments_section}= Summary

{verdict}

// The counts as a table rather than as five large figures: the verdict above is what the section
// is for, and a row of 27pt numbers beside it competes for the same glance.
#block(above: 0.9em, below: 0.6em)[
  #table(
    columns: (1fr, 1fr, 1fr, 1fr, 1fr),
    align: (left, left, left, left, left),
    inset: (x: 4pt, y: 4pt),
    stroke: none,
    table.header(..column-labels(({headline_label}, "Passed", "Failed", "Skipped", {duration_label}))),
    table.hline(stroke: 0.6pt + RULE),
    text(size: 10.5pt, weight: "bold")[#thousands(total)],
    text(size: 10.5pt, weight: "bold", fill: RESULT-COLOUR.passed)[#thousands(count-of("passed"))],
    text(size: 10.5pt, weight: "bold",
         fill: if count-of("failed") > 0 {{ RESULT-COLOUR.failed }} else {{ INK }})[#thousands(count-of("failed"))],
    text(size: 10.5pt, weight: "bold",
         fill: if count-of("skipped") > 0 {{ RESULT-COLOUR.skipped }} else {{ INK }})[#thousands(count-of("skipped"))],
    text(size: 10.5pt, weight: "bold")[#calc.round(seconds / 60.0, digits: 1) min],
  )
]

{criteria}
{acceptance}{environment}
{coverage}
{by_area_section}

#let failures = cases.filter(c => c.status == "failed")

#if failures.len() > 0 [
  == Failures

  #for case in failures [
    #block(above: 0.9em, below: 0.35em)[
      #text(font: MONO, size: 9pt, fill: RESULT-COLOUR.failed, weight: "bold")[#case.full]
    ]
    #if case.detail != "" [
      #block(
        inset: (left: 9pt, rest: 6pt),
        fill: rgb("#f7f8f9"),
        stroke: (left: 1.5pt + RESULT-COLOUR.failed),
        width: 100%,
      )[
        #text(font: MONO, size: 7pt, fill: SLATE)[#raw(case.detail)]
      ]
    ]
  ]
]

{summary_clauses}
// One entry per test, not a table row: a description is prose and needs the width of the page,
// while the figures still line up on the right.
{tests_section}
{closing_clauses}{parts["conformance"]}
"""


def coverage_parts(report: Path | None, floor: str) -> tuple[str, dict[str, float], dict[str, int], float | None]:
    """The coverage section, each area's figure and missed lines, and the line total."""
    if report is None:
        return "", {}, {}, None

    if not report.is_file():
        # Most legs do not instrument, so a missing report is ordinary.
        print(f"no coverage report at {report}; the document omits that section")
        return "", {}, {}, None

    totals, files = read_coverage(report)
    if not totals:
        return "", {}, {}, None

    tree = coverage_tree(files)
    by_area = {row.label: row.lines_cover for row in tree}
    missed = {row.label: row.lines_missed for row in tree}
    return coverage_section(totals, files, floor), by_area, missed, float(totals["Lines"].rstrip("%"))


def collected_environment(path: Path | None) -> list[tuple[str, str]]:
    """The machine record, empty when the run wrote none."""
    if path is None:
        return []
    if not path.is_file():
        print(f"no environment record at {path}; the document says so")
        return []
    return read_environment(path)


def collected_descriptions(path: Path | None) -> dict[str, str]:
    """What the build said each registered test checks, empty when it wrote nothing."""
    if path is None:
        return {}
    if not path.is_file():
        # The build writes it, so its absence means the tree was never configured.
        print(f"no descriptions at {path}; the CMake-declared tests show none")
        return {}
    return read_descriptions(path)


class Leg(NamedTuple):
    """One configuration's contribution to the document: what it tested, measured and ran on.

    A combined report holds one per shipped artefact; a single-configuration run holds one. The
    reading is the same either way, so there is one place where a leg's inputs are turned into
    data and one place where a missing or malformed input is refused.
    """

    configuration: str
    report: Path
    cases: list[Case]
    # The coverage report only when a total was actually read off it. A leg that does not
    # instrument is given a path to a file that is not there, and naming it in the references
    # would document a measurement the document itself says was not taken.
    measured: Path | None
    coverage: str
    area_coverage: dict[str, float]
    area_missed: dict[str, int]
    line_coverage: float | None
    environment: list[tuple[str, str]]
    descriptions: dict[str, str]
    environment_file: Path | None
    descriptions_file: Path | None
    stopped_at_first_failure: bool


class LegError(Exception):
    """A leg's inputs cannot be read. The message is what the caller prints."""


def read_leg(
    report: Path,
    configuration: str,
    coverage_report: Path | None,
    coverage_floor: str,
    environment_file: Path | None,
    descriptions_file: Path | None,
    stopped_at_first_failure: bool,
) -> Leg:
    """Reads everything one configuration contributes, or raises saying what is wrong with it."""
    if not report.is_file():
        raise LegError(f"no test report at {report}")

    cases = read_cases(report)
    if not cases:
        raise LegError(str(EmptyReport(f"{report} holds no test cases")))

    environment = collected_environment(environment_file)
    descriptions = collected_descriptions(descriptions_file)
    coverage, area_coverage, area_missed, line_coverage = coverage_parts(coverage_report, coverage_floor)
    if coverage_report is not None and not coverage and coverage_report.is_file():
        raise LegError(f"{coverage_report} has no TOTAL row; the report format changed")

    return Leg(
        configuration=configuration,
        report=report,
        cases=cases,
        measured=coverage_report if coverage else None,
        coverage=coverage,
        area_coverage=area_coverage,
        area_missed=area_missed,
        line_coverage=line_coverage,
        environment=environment,
        descriptions=descriptions,
        environment_file=environment_file,
        descriptions_file=descriptions_file,
        stopped_at_first_failure=stopped_at_first_failure,
    )


def leg_passed(leg: Leg) -> bool:
    """Whether one configuration's run is a pass, which is no case having failed on it.

    Skipped cases do not decide it and neither does coverage. A configuration that did not
    build has no Leg at all: the caller refuses the document rather than leaving a row out,
    because an absent row reads as nothing wrong.
    """
    return not any(case.status == "failed" for case in leg.cases)


def leg_counts(leg: Leg) -> tuple[int, int, int, int]:
    """Cases, passed, failed and skipped for one configuration."""
    return (
        len(leg.cases),
        sum(1 for case in leg.cases if case.status == "passed"),
        sum(1 for case in leg.cases if case.status == "failed"),
        sum(1 for case in leg.cases if case.status == "skipped"),
    )


def combined_verdict(legs: list[Leg]) -> str:
    """The badge a reader should meet before any prose, over every configuration at once."""
    failed = [leg.configuration for leg in legs if not leg_passed(leg)]
    word, colour = ("FAILED", "RESULT-COLOUR.failed") if failed else ("PASSED", "RESULT-COLOUR.passed")
    return f"""#block(above: 0.9em, below: 1em)[
  #verdict-badge("{word}", {colour})
]
"""


# The headline figures a document covering one configuration uses: its own cases, counted once.
ONE_HEADLINE_TOTALS = """#let total = cases.len()
#let count-of(wanted) = cases.filter(c => c.status == wanted).len()
#let seconds = cases.fold(0.0, (running, c) => running + c.seconds)"""


def combined_headline_totals(legs: list[Leg]) -> str:
    """The headline figures across every configuration, added together.

    Runs of a case rather than distinct cases: a release runs the suite once per configuration,
    and what it executed is the sum. The count of distinct tests is the Coverage total, and
    Acceptance says which of the two each row is.
    """
    tests = sum(len(leg.cases) for leg in legs)
    passed = sum(leg_counts(leg)[1] for leg in legs)
    failed = sum(leg_counts(leg)[2] for leg in legs)
    skipped = sum(leg_counts(leg)[3] for leg in legs)
    seconds = sum(case.seconds for leg in legs for case in leg.cases)
    return (
        f"#let total = {tests}\n"
        f"#let count-of(wanted) = (passed: {passed}, failed: {failed}, skipped: {skipped}).at(wanted)\n"
        f"#let seconds = {seconds:.1f}"
    )


def combined_criteria_prose(legs: list[Leg], line_coverage: float | None, floor: str) -> str:
    """What the verdict was reached by, in a sentence rather than a table of three rows.

    The table said the same thing as the row of counts beside it and the figures below it, so it
    is stated once here and the page keeps the room for the tables that carry data.
    """
    failures = sum(leg_counts(leg)[2] for leg in legs)
    # Named by configuration rather than totalled: the headline counts distinct tests, where a
    # case skipped on one configuration and run on another is not skipped, so a bare total here
    # would read as contradicting it.
    skipped_on = [(leg.configuration, leg_counts(leg)[3]) for leg in legs if leg_counts(leg)[3]]
    measured = [leg.configuration for leg in legs if leg.line_coverage is not None]
    if not measured:
        seen = "No configuration measured coverage, so the floor was not tested."
    elif len(measured) == 1:
        seen = f"Only {measured[0]} measured coverage, and it reached {line_coverage:.2f}% of lines."
    else:
        seen = f"The lowest any configuration measured was {line_coverage:.2f}% of lines."
    outcome = "every case that ran passed" if not failures else f"{tests(failures)} did not pass"
    # A skipped case is not a failure and not a pass: it verified nothing. Saying the suite ran
    # in full and then that cases were skipped states both halves of that and contradicts itself,
    # so the criterion is about running to the end rather than about skips.
    note = (
        ""
        if not skipped_on
        else " Some cases the suite skipped, and a skipped case verifies nothing; Acceptance says where."
    )
    return (
        f"The run reads PASSED only if every case that runs passes on the configuration that ran"
        f" it, no configuration stops at its first failure, and the project's measured line"
        f" coverage holds the {floor}% floor. Individual areas may sit under it, and Coverage says"
        f" which. On this run {outcome} and none stopped early. {seen}{note}"
    )
    return (
        f"The run reads PASSED only if every case passes on every configuration it registers on,"
        f" the whole suite runs, and measured line coverage holds the {floor}% floor. On this run"
        f" {outcome} and the whole suite ran. {seen}{note}"
    )


def acceptance_section(legs: list[Leg], criteria_prose: str = "") -> str:
    """One row per configuration: its verdict, its counts, and the coverage figure beside them.

    The verdict column sits before the counts and not at the end, where it would stand against
    the coverage figure and read as a conclusion drawn from it. Coverage is reported here, not
    gated.
    """
    rows = []
    for leg in legs:
        total, passed, failed, skipped = leg_counts(leg)
        ok = leg_passed(leg)
        figure = "not measured" if leg.line_coverage is None else f"{leg.line_coverage:.2f}%"
        rows.append(
            f"  ({typst_string(leg.configuration)}, {str(ok).lower()}, {total}, {passed},"
            f" {failed}, {skipped}, {typst_string(figure)})"
        )
    body = ",\n".join(rows)
    totals = [sum(column) for column in zip(*(leg_counts(leg) for leg in legs), strict=True)]
    every = all(leg_passed(leg) for leg in legs)
    return f"""== Acceptance

#block(below: 1em, text(size: 8.5pt, fill: SLATE)[
  {criteria_prose}

  One row per configuration this report covers, each tested on the build it was produced from. A
  configuration reads failed if any case failed on it or if it did not build; skipped cases and
  the coverage figure do not enter that. Counts differ between compilers because the suite is not
  uniform. The last row adds the configurations together, so it counts runs of a case; the count
  of distinct tests is in Coverage.
])

#let acceptance = (
{body}
)

#block(below: 1.3em)[
  #table(
    columns: (1fr, auto, auto, auto, auto, auto, auto),
    align: (left, center, right, right, right, right, right),
    inset: (x: 5pt, y: 3.2pt),
    stroke: none,
    table.header(..column-labels((
      "Configuration", "Result", "Cases", "Passed", "Failed", "Skipped", "Lines covered",
    ))),
    table.hline(stroke: 0.6pt + RULE),
    ..acceptance.map(((name, ok, total, passed, failed, skipped, cover)) => (
      text(size: 8.5pt)[#name],
      box(
        stroke: 0.9pt + (if ok {{ RESULT-COLOUR.passed }} else {{ RESULT-COLOUR.failed }}),
        radius: 2pt, inset: (x: 5pt, y: 2.5pt),
        text(size: 7pt, weight: "bold", tracking: 0.8pt,
             fill: if ok {{ RESULT-COLOUR.passed }} else {{ RESULT-COLOUR.failed }})[
          #(if ok {{ "PASSED" }} else {{ "FAILED" }})
        ],
      ),
      text(size: 8.5pt)[#thousands(total)],
      text(size: 8.5pt, fill: RESULT-COLOUR.passed)[#thousands(passed)],
      text(size: 8.5pt, fill: if failed > 0 {{ RESULT-COLOUR.failed }} else {{ SLATE }})[#thousands(failed)],
      text(size: 8.5pt, fill: if skipped > 0 {{ RESULT-COLOUR.skipped }} else {{ SLATE }})[#thousands(skipped)],
      text(size: 8.5pt, fill: SLATE)[#cover],
    )).flatten(),
    table.hline(stroke: 0.6pt + RULE),
    text(size: 8.5pt, weight: "bold")[All],
    box(
      stroke: 0.9pt + (if {str(every).lower()} {{ RESULT-COLOUR.passed }} else {{ RESULT-COLOUR.failed }}),
      radius: 2pt, inset: (x: 5pt, y: 2.5pt),
      text(size: 7pt, weight: "bold", tracking: 0.8pt,
           fill: if {str(every).lower()} {{ RESULT-COLOUR.passed }} else {{ RESULT-COLOUR.failed }})[
        #(if {str(every).lower()} {{ "PASSED" }} else {{ "FAILED" }})
      ],
    ),
    text(size: 8.5pt, weight: "bold")[#thousands({totals[0]})],
    text(size: 8.5pt, weight: "bold")[#thousands({totals[1]})],
    text(size: 8.5pt, weight: "bold")[#thousands({totals[2]})],
    text(size: 8.5pt, weight: "bold")[#thousands({totals[3]})],
    text(size: 8.5pt, fill: SLATE)[#sym.dash.em],
  )
]
"""


# What a machine reports per job rather than per image, so it never groups configurations.
# What a cell says when a configuration measured nothing, which is not a figure of zero.
NOTHING_MEASURED = "\u2013"

# Recorded by the run but not part of what the tests ran against: typst typesets this document
# and the host names whichever runner picked the job up. Neither says anything about the
# software under test.
NOT_THE_ENVIRONMENT = ("typst", "Host")


# Where a recorded value may break across lines. A compiler version string is one long token
# with no spaces, so without these it runs out of the box that holds it.
WRAP_AFTER = "+~()/,"


# Values the recorder captures as a whole --version line, where only the number identifies the
# tool. The line itself stays in test-environment.tsv, which Test deliverables names with its
# digest, so nothing is lost by showing the number here.
VERSION_KEYS = ("Compiler", "CMake", "Ninja", "Python", "Node", "Conan")
_PARENTHESISED = re.compile(r"\([^)]*\)")
_VERSION = re.compile(r"\d+(?:\.\d+)+")


def short_version(key: str, value: str) -> str:
    """The version out of a tool's --version line, or the line as recorded if it holds none.

    The parenthesised part is a distribution's own package version, which is not the tool's, so
    it goes before the number is looked for: g++ prints both and only the second is the compiler.
    """
    if key not in VERSION_KEYS:
        return value
    found = _VERSION.search(_PARENTHESISED.sub(" ", value))
    return found.group(0) if found else value


def wrappable(value: str) -> str:
    """Puts zero-width spaces into a recorded value so a long one wraps inside its column."""
    out = []
    for character in value:
        out.append(character)
        if character in WRAP_AFTER:
            out.append(ZERO_WIDTH_SPACE)
    return "".join(out)


def environments_of(legs: list[Leg]) -> str:
    """Every configuration's machine and toolchain, with each value stated once where it can be.

    De-duplicated per key rather than per environment: on real runs the legs differ in operating
    system, processor and tool versions, so grouping by distinct environment produces one group
    per configuration and saves nothing. A key every leg agrees on is written once.
    """
    keys: list[str] = []
    for leg in legs:
        for name, _ in leg.environment:
            if name not in keys and name not in NOT_THE_ENVIRONMENT:
                keys.append(name)

    def raw(leg: Leg, key: str) -> str:
        return next((v for n, v in leg.environment if n == key), "not reported")

    # Shorten a version only where that hides nothing. Three legs record Ninja as
    # 1.13.2.git.kitware.jobserver-pipe-1 and two as 1.13.2; shortening both to the number makes
    # a real difference between the machines look like agreement, under a paragraph that says a
    # value written once was recorded identically everywhere.
    hides = {
        k
        for k in keys
        if len({raw(leg, k) for leg in legs}) > 1 and len({short_version(k, raw(leg, k)) for leg in legs}) == 1
    }

    def value(leg: Leg, key: str) -> str:
        return raw(leg, key) if key in hides else short_version(key, raw(leg, key))

    shared = [k for k in keys if len({raw(leg, k) for leg in legs}) == 1]
    varying = [k for k in keys if k not in shared]

    shared_rows = ",\n".join(f"  ({typst_string(k)}, {typst_string(value(legs[0], k))})" for k in shared)
    shared_block = (
        f"""#block(below: 1.2em)[
  #grid(
    columns: (auto, 1fr),
    row-gutter: 3.5pt,
    column-gutter: 14pt,
    ..(
{shared_rows}
    ).map(((name, v)) => (label-text(name), text(font: MONO, size: 8pt, fill: SLATE)[#v])).flatten()
  )
]
"""
        if shared
        else ""
    )

    blocks = []
    for leg in legs:
        rows = ",\n".join(f"      ({typst_string(k)}, {typst_string(wrappable(value(leg, k)))})" for k in varying)
        blocks.append(
            f"""#block(width: 100%, height: {{box-height}}, breakable: false,
        stroke: 0.5pt + RULE, radius: 2pt, inset: (x: 8pt, y: 6pt))[
  #text(size: 9pt, weight: "bold")[{typst_string(leg.configuration)[1:-1]}]
  #block(above: 0.6em)[
    #grid(
      columns: (auto, 1fr),
      row-gutter: 1.6pt,
      column-gutter: 10pt,
      ..(
{rows}
      ).map(((name, v)) => (label-text(name), text(font: MONO, size: 6.8pt, fill: SLATE)[#v])).flatten()
    )
  ]
]
"""
        )

    return f"""= Test environments

#block(below: 1.2em, text(size: 8.5pt, fill: SLATE)[
  The item under test is Sen at the code revision above, built in each configuration below and
  run on a machine whose system, processor and toolchain are recorded with it. Figures from
  one configuration do not carry to another,
  and a duration means nothing without the processors it had. Everything here was read from the
  machine by the run itself, not stated by hand. A value every configuration shares is written
  once; the rest are given per configuration.
])

{"== Shared by every configuration" if shared else ""}

{shared_block}
{"== Configurations" if varying else ""}

// One height for all of them: boxes that differ only because a version string wrapped read
// as though they held different things. Kept above what this run needs, because the values
// come from the machines and a longer one on another run would otherwise overlap.
#let box-height = 3.5cm

#grid(
  columns: (1fr, 1fr),
  column-gutter: 9pt,
  row-gutter: 9pt,
{",".join(block.lstrip("#") for block in blocks)}
)
"""


def read_manifest(path: Path, coverage_floor: str, stopped_at_first_failure: bool) -> list[Leg]:
    """Reads one leg per line: configuration, report, coverage, environment, descriptions.

    Tab separated, an empty field meaning the leg has none. A line naming a report that is not
    there stops the document rather than being dropped, because a configuration missing from a
    release report reads as nothing wrong.
    """
    legs: list[Leg] = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        fields = (line.split("\t") + [""] * 5)[:5]
        configuration, report, coverage, environment, descriptions = (f.strip() for f in fields)
        if not configuration or not report:
            raise LegError(f"{path}:{number} names no configuration or no report")
        legs.append(
            read_leg(
                Path(report),
                configuration,
                Path(coverage) if coverage else None,
                coverage_floor,
                Path(environment) if environment else None,
                Path(descriptions) if descriptions else None,
                stopped_at_first_failure,
            )
        )
    if not legs:
        raise LegError(f"{path} lists no configurations")

    if not matrix_fits(len(legs)):
        raise LegError(
            f"{len(legs)} configurations do not fit the results matrix, which holds "
            f"{matrix_capacity()}. Widen the page or drop the per-configuration durations."
        )
    return legs


# MSVC spells a template argument differently from gcc and clang, so the same typed test
# registers under a different name: `int * __ptr64` against `int*`, backtick-quoted anonymous
# namespaces, and 10 against 10ul for the same size_t. Matching a case across configurations
# folds those; the name shown is still the one that configuration wrote.
_SPELLINGS = (
    (re.compile(r"__ptr64"), ""),
    (re.compile(r"`anonymous namespace'"), "(anonymous namespace)"),
    (re.compile(r"\b(?:class|struct|enum) "), ""),
    (re.compile(r"\(anonymous namespace\)::"), ""),
    (re.compile(r"\b(\d+)(?:ull|ul|ll|[ul])\b"), r"\1"),
    (re.compile(r"\s+"), ""),
)


def match_key(full_name: str) -> str:
    """The key one case is matched by across configurations.

    Folds only the compiler's own spelling of a name, never two tests into one: applied within a
    single configuration it collapses nothing, which test_folding_never_merges_two_tests holds.
    """
    for pattern, replacement in _SPELLINGS:
        full_name = pattern.sub(replacement, full_name)
    return full_name


def union_cases(legs: list[Leg]) -> list[Case]:
    """Every case once, carrying its worst outcome across the configurations that ran it.

    Failed on any configuration is failed: a test that passes on four builds and fails on one
    has not passed. Skipped stands only where no configuration executed it. The duration kept is
    the longest, so a reader looking for slow tests is not shown the fastest machine's figure.
    """
    worst: dict[str, Case] = {}
    for leg in legs:
        for case in leg.cases:
            key = match_key(case.full_name)
            seen = worst.get(key)
            if seen is None:
                worst[key] = case
                continue
            rank = {"failed": 2, "passed": 1, "skipped": 0}
            keep = case if rank[case.status] > rank[seen.status] else seen
            worst[key] = keep._replace(seconds=max(seen.seconds, case.seconds))
    return list(worst.values())


# The lead over the area table. A document covering one configuration says "the run"; a combined
# one must not, because the counts there are the union and a failure is a failure anywhere.
ONE_BY_AREA_LEAD = """#block(below: 1em, text(size: 8.5pt, fill: SLATE)[
  Every test the run reported, grouped by the part of Sen it is declared in. A C++ case carries its
  description in the source beside it; a vitest or bats case is named with one; a test registered in
  CMake is given one there. Where a figure for covered lines appears it is that area's, so what is
  tested and how much of it is reached can be read on one line.
])"""


BUILD_TYPE_SHORT = {"RelWithDebInfo": "RelDbg", "MinSizeRel": "MinSize", "Release": "Rel", "Debug": "Dbg"}


def short_labels(legs: list[Leg]) -> list[str]:
    """A short name per configuration, for the strip under every test.

    A token every configuration shares tells a reader nothing here, so it is dropped, and the long
    build-type names are abbreviated. A configuration left with nothing keeps its full name.
    """
    split = [leg.configuration.replace(" ", "-").split("-") for leg in legs]
    shared = {token for token in split[0] if all(token in other for other in split)} if len(split) > 1 else set()
    labels = []
    for tokens, leg in zip(split, legs, strict=True):
        kept = [BUILD_TYPE_SHORT.get(token, token) for token in tokens if token not in shared]
        labels.append(" ".join(kept) if kept else leg.configuration)
    return labels


# The enumeration as a document covering one configuration writes it: every test with its
# description and its single result, grouped by where it is declared.
ONE_TESTS_SECTION = """= The tests

#block(below: 1.4em, text(size: 8.5pt, fill: SLATE)[
  Every test the run reported, in full: what it checks, the requirements it names where it names
  any, its result and how long it took. Grouped by the part of Sen it is declared in, and within
  that by the suite, spec file or directory it sits in.
])

#for group in groups [
  == #raw(group)

  #for area in areasIn(group) [
    === #raw(leafOf(area))

    #let list = inArea(area)
    #for section in sectionsIn(list) [
      ==== #raw(section)
      #entriesOf(list.filter(c => c.section == section))
    ]
  ]
]

#if unplaced.len() > 0 [
  == Tests whose area was not resolved

  #block(below: 1.3em, text(size: 8.5pt, fill: SLATE)[
    Registered in CMake under a name built from variables, which cannot be traced back to a
    directory without evaluating the build.
  ])

  #for section in sectionsIn(unplaced) [
    #if section != "" [==== #raw(section)]
    #entriesOf(unplaced.filter(c => c.section == section))
  ]
]
"""


def as_section(clause: str) -> str:
    """A clause's heading promoted one level, so it stands as a section rather than under one.

    The clause builders write for Summary, where these are subsections. A combined report runs
    long enough that they read better after the results than buried in the summary.
    """
    head, newline, rest = clause.partition("\n")
    return (head[1:] if head.startswith("==") else head) + newline + rest


def combined_tests_sections() -> str:
    """Results as a matrix, and the descriptions under it, with a name linking to its description.

    Split because they are read differently: the matrix is scanned for a configuration that
    disagrees with the others, and a description is looked up once the scan has found one. Keeping
    them together makes the matrix as long as the prose.
    """
    return """= Test results

#block(below: 1em, text(size: 8.5pt, fill: SLATE)[
  One row per test and one column per configuration, carrying the result and the seconds it took.
  A test name links to its description. Grouped by the part of Sen the test is declared in, and
  within that by the suite, spec file or directory it sits in.
])

#block(below: 1.2em)[
  #text(size: 8pt, fill: SLATE)[
    #resultCell((status: "passed", seconds: 0.0)) passed #h(10pt)
    #resultCell((status: "failed", seconds: 0.0)) failed #h(10pt)
    #resultCell((status: "skipped", seconds: 0.0)) skipped by the suite #h(10pt)
    #resultCell((status: "absent", seconds: 0.0)) not registered on that configuration
  ]
]

#for group in groups [
  == #raw(group)

  #for area in areasIn(group) [
    === #raw(leafOf(area))

    #let list = inArea(area)
    #for section in sectionsIn(list) [
      ==== #raw(section)
      #resultsOf(list.filter(c => c.section == section))
    ]
  ]
]

#if unplaced.len() > 0 [
  == Tests whose area was not resolved

  #block(below: 1.3em, text(size: 8.5pt, fill: SLATE)[
    Registered in CMake under a name built from variables, which cannot be traced back to a
    directory without evaluating the build.
  ])

  #for section in sectionsIn(unplaced) [
    #if section != "" [==== #raw(section)]
    #resultsOf(unplaced.filter(c => c.section == section))
  ]
]

= Test descriptions

#block(below: 1.4em, text(size: 8.5pt, fill: SLATE)[
  What each test checks, and the requirements it names where it names any. The results are in the
  matrix above; this says what each test checks.
])

#for group in groups [
  == #raw(group)

  #for area in areasIn(group) [
    === #raw(leafOf(area))

    #let list = inArea(area)
    #for section in sectionsIn(list) [
      ==== #raw(section)
      #describedOf(list.filter(c => c.section == section))
    ]
  ]
]

#if unplaced.len() > 0 [
  == Tests whose area was not resolved

  #for section in sectionsIn(unplaced) [
    #if section != "" [==== #raw(section)]
    #describedOf(unplaced.filter(c => c.section == section))
  ]
]
"""


def label_parts(legs: list[Leg]) -> list[tuple[str, str]]:
    """Each column heading split in two, so the build type always sits on the second line.

    A heading left to wrap where it fits puts the build type in a different place in every
    column, and that is the word a reader compares across them.
    """
    parts = []
    for label in short_labels(legs):
        tokens = label.split()
        if len(tokens) > 1 and tokens[-1] in set(BUILD_TYPE_SHORT.values()):
            parts.append((" ".join(tokens[:-1]), tokens[-1]))
        else:
            parts.append((label, ""))
    return parts


AREA_TABLE = """#table(
  columns: (1fr, 1.9cm, 1.9cm, 1.7cm, 1.8cm, 2cm),
  align: (left, right, right, right, right, right),
  inset: (x: 4pt, y: 3.5pt),
  stroke: none,
  table.header(..column-labels(("Area", "Tests", "Passed", "Failed", "Skipped", "Covered"))),
  table.hline(stroke: 0.6pt + RULE),
  ..groups
    .map(group => (
      indexRow(group, group, inGroup(group), 0),
      ..areasIn(group).map(area => indexRow(area, leafOf(area), inArea(area), 1)),
    ))
    .flatten(),
  table.hline(stroke: 0.6pt + RULE),
  // Every case the run reported, which is more than the groups above add up to when one of them
  // could not be resolved to a directory; Deviations says how many.
  ..indexRow("", "Total", cases, 0)
)"""


def one_by_area_section() -> str:
    """The area index as a document covering one configuration writes it."""
    return "== Tests by area\n\n" + ONE_BY_AREA_LEAD + "\n\n" + AREA_TABLE + "\n"


def combined_by_area_section(legs: list[Leg]) -> str:
    """Tests and coverage per area in one table, a coverage column per configuration.

    They were two tables saying things about the same rows, and the area tree here reads better
    than a flat list of paths: a group is a line of its own and its directories sit under it.
    """
    keys: list[str] = []
    for leg in legs:
        for area in leg.area_coverage:
            if area not in keys:
                keys.append(area)
    rows = []
    for area in sorted(keys):
        cells = []
        for leg in legs:
            got = leg.area_coverage.get(area)
            cells.append(
                f"({typst_string(NOTHING_MEASURED)}, -1.0)"
                if got is None
                else f"({typst_string(f'{got:.1f}%')}, {got:.4f})"
            )
        rows.append(f"  {typst_string(area)}: ({', '.join(cells)},),")
    by_area = "\n".join(rows)

    nothing = ", ".join(f"({typst_string(NOTHING_MEASURED)}, -1.0)" for _ in legs)
    totals = ", ".join(
        f"({typst_string(NOTHING_MEASURED)}, -1.0)"
        if leg.line_coverage is None
        else f"({typst_string(f'{leg.line_coverage:.2f}%')}, {leg.line_coverage:.4f})"
        for leg in legs
    )
    silent = [leg.configuration for leg in legs if leg.line_coverage is None]
    note = "" if not silent else " A dash is a configuration that measured nothing, not a figure of zero."
    width = "1.95cm"
    return f"""== Coverage

#block(below: 0.8em, text(size: 8.5pt, fill: SLATE)[
  Every test any configuration reported, counted once and grouped by where it is declared,
  beside what each configuration's own binaries covered of that part. A case counts
  as failed if it failed on any configuration; Test results says which.{note}
])

#let areaCoverageBy = (
{by_area}
)

// The same row as the index it replaces, with a coverage cell per configuration rather than one.
#let areaRow(key, shown, list, level) = {{
  let failed = countIn(list, "failed")
  let heavy = if level == 0 {{ "bold" }} else {{ "regular" }}
  // Only the total row takes the overall figures. An area nobody measured is a dash: showing
  // the project total against one directory would read as that directory's coverage.
  let covers = if key in areaCoverageBy {{ areaCoverageBy.at(key) }}
    else if key == "" {{ ({totals},) }} else {{ ({nothing},) }}
  (
    block(inset: (left: level * 12pt))[
      #text(font: MONO, size: if level == 0 {{ 8.5pt }} else {{ 8pt }}, weight: heavy,
            fill: if level == 0 {{ INK }} else {{ SLATE }})[#shown]
    ],
    text(size: 8pt, weight: heavy)[#thousands(list.len())],
    // Failed here is failed on at least one configuration, which no other table says. Passed
    // would be this column subtracted from Tests, and skipped-everywhere is not what Acceptance
    // means by skipped, so neither is carried.
    if failed > 0 {{ text(size: 8pt, weight: heavy, fill: RESULT-COLOUR.failed)[#thousands(failed)] }} else {{
      text(size: 8pt, fill: SLATE)[0]
    }},
    // Grey, not the amber the results matrix uses: there a dash means the case never
    // registered on that configuration, here it means nobody measured that area.
    ..covers.map(c => if c.at(1) < 0 {{ text(size: 7.5pt, fill: SLATE)[#c.at(0)] }} else {{
      text(size: 7.5pt, weight: heavy, fill: cover-colour(c.at(1)))[#c.at(0)]
    }}),
  )
}}

#table(
  columns: (1fr, 1.2cm, 1.1cm, {", ".join([width] * len(legs))}),
  align: (left, right, right, {", ".join(["right"] * len(legs))}),
  inset: (x: 4pt, y: 1.75pt),
  stroke: none,
  table.header(
    label-text("Area"), config-label("Distinct tests"), config-label("Failed"),
    ..configurations.map(c => align(right)[
      #cover-label(if c.at(1) != "" {{ c.at(0) + " " + c.at(1) }} else {{ c.at(0) }})
    ]),
  ),
  table.hline(stroke: 0.6pt + RULE),
  ..groups
    .map(group => (
      areaRow(group, group, inGroup(group), 0),
      ..areasIn(group).map(area => areaRow(area, leafOf(area), inArea(area), 1)),
    ))
    .flatten(),
  table.hline(stroke: 0.6pt + RULE),
  ..areaRow("", "Total", cases, 0)
)
"""


def read_registrations_for(sources: Path | None, build: Path | None) -> Registrations | None:
    """What the build registered, with the names only its generated ctest files can resolve.

    A test name built from CMake variables cannot be read off the CMakeLists; the ctest files the
    build writes have evaluated them, so they are read when a build tree is at hand.
    """
    if sources is None:
        return None

    registrations = scan_registrations(sources)
    if registrations is not None and build is not None:
        named = scan_ctest(build, sources)
        if named:
            registrations.exact.update(named)
            print(f"{len(named)} tests placed by the build's own ctest files")
    return registrations


def cover_identity(
    commit: str,
    ref: str,
    run: str,
    configuration: str,
    started: str,
    report: Path,
) -> list[tuple[str, str]]:
    """What the cover says this document is, which is where it came from.

    Not a version: a version names a release, and which release a commit becomes is decided after
    the run this reports on. Every row here can be checked against something.
    """
    return [
        ("Code revision", commit),
        ("Ref", ref or "not recorded"),
        ("Build", run or "not recorded"),
        ("Configuration", configuration),
        ("Generator", f"sha256 {generator_digest()}"),
        ("Tests ran", started or "not recorded"),
        ("Generated", datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")),
        ("Source report", f"{report.name}  sha256 {file_digest(report)}"),
    ]


def lowest_coverage(legs: list[Leg]) -> tuple[float | None, dict[str, float], dict[str, int]]:
    """The lowest figure any configuration measured, overall and per area.

    A floor claimed for a release has to hold for every configuration in it, not for the best one,
    so the figures a combined document carries are the worst rather than an average.
    """
    figures = [leg.line_coverage for leg in legs if leg.line_coverage is not None]
    areas: dict[str, float] = {}
    missed: dict[str, int] = {}
    for leg in legs:
        for area, value in leg.area_coverage.items():
            areas[area] = min(areas.get(area, value), value)
        for area, count in leg.area_missed.items():
            missed[area] = max(missed.get(area, count), count)
    return (min(figures) if figures else None), areas, missed


class Shape(NamedTuple):
    """The pieces of a document that depend on how many configurations it covers."""

    line_coverage: float | None
    area_coverage: dict[str, float]
    area_missed: dict[str, int]
    coverage: str
    acceptance: str
    environment: str
    environments_section: str
    per_leg: list[tuple[str, dict[str, Case]]] | None
    by_area_section: str
    tests_section: str
    configuration_names: str


def shape_of(legs: list[Leg], coverage_floor: str) -> Shape:
    """What a document covering these configurations is made of.

    One configuration keeps the shape the report has always had. Several replace the sections
    that can only speak for one of them.
    """
    if len(legs) == 1:
        return Shape(
            line_coverage=legs[0].line_coverage,
            area_coverage=legs[0].area_coverage,
            area_missed=legs[0].area_missed,
            coverage=legs[0].coverage,
            acceptance="",
            environment=environment_section(legs[0].environment),
            environments_section="",
            per_leg=None,
            by_area_section=one_by_area_section(),
            tests_section=ONE_TESTS_SECTION,
            configuration_names="()",
        )

    line_coverage, area_coverage, area_missed = lowest_coverage(legs)
    names = ", ".join(f"({typst_string(head)}, {typst_string(tail)})" for head, tail in label_parts(legs))
    return Shape(
        line_coverage=line_coverage,
        area_coverage=area_coverage,
        area_missed=area_missed,
        coverage="",
        acceptance=acceptance_section(legs, combined_criteria_prose(legs, line_coverage, coverage_floor)),
        environment="",
        environments_section=environments_of(legs),
        per_leg=[
            (label, {match_key(case.full_name): case for case in leg.cases})
            for label, leg in zip(short_labels(legs), legs, strict=True)
        ],
        by_area_section=combined_by_area_section(legs),
        tests_section=combined_tests_sections(),
        configuration_names="(" + names + ",)",
    )


def read_legs(
    manifest: Path | None,
    positional: list[str],
    coverage_report: Path | None,
    coverage_floor: str,
    environment_file: Path | None,
    descriptions_file: Path | None,
    stopped_at_first_failure: bool,
) -> list[Leg]:
    """Every configuration this document covers, from a manifest or from the one named directly."""
    if manifest is not None:
        return read_manifest(manifest, coverage_floor, stopped_at_first_failure)
    return [
        read_leg(
            Path(positional[0]),
            positional[2] if len(positional) > 2 else "not stated",
            coverage_report,
            coverage_floor,
            environment_file,
            descriptions_file,
            stopped_at_first_failure,
        )
    ]


def main() -> int:
    """Writes the document for the report named on the command line."""
    arguments = sys.argv[1:]
    stopped_at_first_failure = STOP_FLAG in arguments
    try:
        flags, positional = split_arguments(arguments)
    except ValueError as unknown:
        print(unknown, file=sys.stderr)
        return 2

    def path_flag(flag: str) -> Path | None:
        value = flags.get(flag)
        return Path(value) if value else None

    coverage_report = path_flag(COVERAGE_FLAG)
    coverage_floor = flags.get(FLOOR_FLAG) or DEFAULT_FLOOR
    descriptions_file = path_flag(DESCRIPTIONS_FLAG)
    ref = flags.get(REF_FLAG, "")
    run = flags.get(RUN_FLAG, "")
    environment_file = path_flag(ENVIRONMENT_FLAG)
    build = path_flag(BUILD_FLAG)
    logo_file = path_flag(LOGO_FLAG)
    organization = flags.get(ORGANIZATION_FLAG) or ISSUER
    approver = flags.get(APPROVER_FLAG, "")

    # With a manifest there is no single source report to name, so the output takes its place.
    manifest = path_flag(LEGS_FLAG)
    if len(positional) < (1 if manifest is not None else 2):
        print(
            "usage: render_test_document.py <report.xml> <out.typ> [configuration] [commit]"
            f" [sources] [{STOP_FLAG}] [{COVERAGE_FLAG}<coverage.txt>] [{FLOOR_FLAG}<percent>]"
            f" [{DESCRIPTIONS_FLAG}<test-descriptions.tsv>]"
            f" [{ENVIRONMENT_FLAG}<test-environment.tsv>] [{BUILD_FLAG}<build directory>]"
            f" [{LOGO_FLAG}<logo.svg>] [{ORGANIZATION_FLAG}<issuer>] [{APPROVER_FLAG}<name>]"
            f" [{REF_FLAG}<branch or tag>] [{RUN_FLAG}<build number>]\n"
            f"   or: render_test_document.py {LEGS_FLAG}<manifest.tsv> <out.typ> [commit] [sources]"
            " [the flags above that are not per-configuration]",
            file=sys.stderr,
        )
        return 2

    # A manifest puts the output first, since there is no single source report to name.
    at = 0 if manifest is not None else 1
    out = Path(positional[at])
    commit = positional[at + 1] if len(positional) > at + 1 else "not stated"
    sources = Path(positional[at + 2]) if len(positional) > at + 2 else None
    if manifest is None:
        commit = positional[3] if len(positional) > 3 else "not stated"
        sources = Path(positional[4]) if len(positional) > 4 else None

    try:
        legs = read_legs(
            manifest,
            positional,
            coverage_report,
            coverage_floor,
            environment_file,
            descriptions_file,
            stopped_at_first_failure,
        )
    except LegError as unreadable:
        print(unreadable, file=sys.stderr)
        return 1

    combined = len(legs) > 1
    report = legs[0].report
    configuration = legs[0].configuration
    # A test that passed on four configurations and failed on one has not passed, so the
    # enumeration carries each case's worst outcome and the per-configuration detail is above it.
    cases = union_cases(legs) if combined else legs[0].cases
    recorded_environment = legs[0].environment
    descriptions = {name: text for leg in legs for name, text in leg.descriptions.items()}
    measured = legs[0].measured

    shape = shape_of(legs, coverage_floor)
    line_coverage, area_coverage = shape.line_coverage, shape.area_coverage
    area_missed = shape.area_missed
    coverage, acceptance, environment = shape.coverage, shape.acceptance, shape.environment
    environments_section, per_leg = shape.environments_section, shape.per_leg
    by_area_section, tests_section = shape.by_area_section, shape.tests_section
    configuration_names = shape.configuration_names

    started = run_span(legs) if combined else run_started(report)
    # The cover says where this document came from, not what the software is called: a version
    # names a release, and which release a commit becomes is decided after the run that tested it.
    # It stays in the environment table below as the build property it is.
    meta = cover_identity(commit, ref, run, configuration, started, report)

    annotations = scan(sources) if sources is not None else {}
    registrations = read_registrations_for(sources, build)

    if measured is not None:
        meta.append(("Coverage report", f"{measured.name}  sha256 {file_digest(measured)}"))

    # The cover identifies the document, not the configurations in it: with several, naming one
    # configuration or one source report would be naming an arbitrary member. Test deliverables
    # lists every file, per configuration, with its digest.
    if combined:
        dropped = ("Configuration", "Source report", "Coverage report")
        meta = [row for row in meta if row[0] not in dropped]

    matched = [annotation_for(annotations, *split_name(case.classname, case.name)[:2]) for case in cases]
    unresolved = sum(
        1 for case, annotation in zip(cases, matched, strict=True) if not placement(case, annotation, registrations)[2]
    )
    verdict, criteria = conclusion_block(cases, line_coverage, coverage_floor, stopped_at_first_failure)
    if combined:
        verdict = combined_verdict(legs)
        criteria = ""
    # Areas a test was declared in that the coverage measurement never reported on.
    tested = {placement(case, annotation, registrations)[2] for case, annotation in zip(cases, matched, strict=True)}
    unmeasured = sorted(area for area in tested if area and area not in area_coverage)
    deviations = deviations_section(
        cases,
        unresolved,
        sum(leg_counts(leg)[3] for leg in legs) if combined else None,
        unmeasured,
        sum(1 for annotation in matched if not (annotation and annotation.requirements)),
    )
    logo = place_logo(logo_file, out)
    dependency_set = next((value for name, value in recorded_environment if name == "Dependencies"), "not recorded")
    clause = {
        "information": document_information(organization, approver, commit),
        "references": references(report, measured, dependency_set, len(legs)),
        "glossary": glossary(),
        "blockers": blockers(),
        "risks": residual_risks(cases, line_coverage, coverage_floor, area_coverage, area_missed),
        "deliverables": (
            combined_deliverables(legs)
            if combined
            else deliverables(report, measured, descriptions_file, environment_file)
        ),
        "assets": reusable_assets(),
        "lessons": lessons(),
        "conformance": conformance(approver, combined),
    }

    # One configuration keeps these under Summary. Several make a long report, where they read
    # better as sections of their own after the enumeration they comment on.
    closing = [
        deviations,
        clause["blockers"],
        clause["risks"],
        clause["deliverables"],
        clause["assets"],
        clause["lessons"],
    ]
    if combined:
        summary_clauses = ""
        closing_clauses = "\n".join(as_section(part) for part in closing) + "\n"
        headline_totals = combined_headline_totals(legs)
        headline_label = '"Test runs"'
        duration_label = '"Total case time"'
        scope_opening = (
            f"This is the completion report for one run of Sen's test suite on each of the"
            f" {len(legs)} configurations this report covers."
        )
    else:
        summary_clauses = "\n".join(closing)
        closing_clauses = ""
        headline_totals = ONE_HEADLINE_TOTALS
        headline_label = '"Tests"'
        duration_label = '"Duration"'
        scope_opening = "This is the completion report for one run of Sen's test suite."

    out.write_text(
        render(
            cases,
            meta,
            annotations=annotations,
            registrations=registrations,
            logo=logo,
            coverage=coverage,
            descriptions=descriptions,
            area_coverage=area_coverage,
            line_coverage=line_coverage,
            verdict=verdict,
            criteria=criteria,
            acceptance=acceptance,
            by_area_section=by_area_section,
            tests_section=tests_section,
            summary_clauses=summary_clauses,
            closing_clauses=closing_clauses,
            headline_totals=headline_totals,
            headline_label=headline_label,
            scope_opening=scope_opening,
            duration_label=duration_label,
            configuration_names=configuration_names,
            environments_section=environments_section,
            per_leg=per_leg,
            environment=environment,
            clause=clause,
        ),
        encoding="utf-8",
    )
    annotated = sum(1 for annotation in matched if annotation and annotation.description)
    # A test registered in CMake is described by the build, not by an annotation.
    declared = sum(
        1
        for case, annotation in zip(cases, matched, strict=True)
        if annotation is None and split_name(case.classname, case.name)[1] in descriptions
    )
    # A spec case is its own description.
    spoken = sum(1 for case in cases if is_spec(split_name(case.classname, case.name)[0]))
    traced = sum(1 for annotation in matched if annotation and annotation.requirements)
    print(
        f"wrote {out} for {len(cases)} cases: {annotated + spoken + declared} describe what they"
        f" check ({spoken} by their own sentence, {declared} by the build), {traced} name a"
        " requirement"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
