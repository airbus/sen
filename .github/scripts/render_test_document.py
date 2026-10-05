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

import hashlib
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


def display_name(name: str, limit: int = NAME_LIMIT) -> str:
    """The name as the document shows it: the identifier, without gtest's parameter dump."""
    cut = name.split(PARAMETER_DUMP, 1)[0].rstrip()
    if len(cut) > limit:
        return cut[:limit] + "..."
    return cut


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
) -> str:
    """The cases as a typst array of dictionaries."""
    rows = []
    for case in cases:
        suite, name, full = split_name(case.classname, case.name)
        annotation = annotation_for(annotations, suite, name)

        section, kind, area = placement(case, annotation, registrations)

        limit = SPEC_NAME_LIMIT if kind == "spec" else NAME_LIMIT
        # A test macro carries its own annotation; one registered in CMake carries what the
        # CMake helper was given.
        describes = annotation.description if annotation else (descriptions or {}).get(name, "")
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


def environment_section(recorded: list[tuple[str, str]]) -> str:
    """The environment as a typst table, or a sentence saying none was recorded."""
    if not recorded:
        return (
            "#block(below: 1.4em, text(size: 8.5pt, fill: SLATE)[\n"
            "  The run did not record the machine it happened on, so this report cannot state it.\n"
            "])\n"
        )

    rows = ",\n".join(f"  ({typst_string(name)}, {typst_string(value)})" for name, value in recorded)
    return f"""#let environment = (
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


def deviations_section(cases: list[Case], unresolved: int) -> str:
    """What the run did not account for, which the totals do not show."""
    skipped = sum(1 for case in cases if case.status == "skipped")

    items = []
    if skipped:
        items.append(f"{tests(skipped)} {were(skipped)} skipped, and so verified nothing in this run.")
    if unresolved:
        items.append(
            "Registered under a name built from CMake variables, which cannot be traced to a"
            f" directory without evaluating the build: {tests(unresolved)}, listed last and outside"
            " any area."
        )
    items.append(
        "Components excluded from the coverage measurement show no figure against their area:"
        " the generated sources, and the explorer, shell and rest components, which are deprecated."
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


def references(report: Path, coverage: Path | None, dependencies: str) -> str:
    """Clause 6.4.3.2: what this document rests on, and where to find it."""
    items = [
        f"The test report this document enumerates: `{report.name}`, written by ctest during the run.",
    ]
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


def residual_risks(cases: list[Case], line_coverage: float | None, floor: str, area_coverage: dict[str, float]) -> str:
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
        thin = sorted(
            (area for area, value in area_coverage.items() if value < float(floor) and "/" in area),
            key=lambda area: area_coverage[area],
        )[:5]
        items.append(
            f"{100.0 - line_coverage:.2f}% of measured lines are reached by no test. A defect on"
            " one of them cannot be found by this suite."
        )
        if thin:
            items.append(
                "The thinnest areas, which carry the most of that risk: "
                + ", ".join(f"{area} at {area_coverage[area]:.1f}%" for area in thin)
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


def conformance(approver: str) -> str:
    """Where each clause 6.4 item is answered, so the claim can be checked."""
    rows = [
        ("6.4.2.2", "Unique identification", "Cover, and Document information"),
        ("6.4.2.3", "Issuing organization", "Document information"),
        ("6.4.2.4", "Approval authority", "Document information" if approver else "Not claimed"),
        ("6.4.2.5", "Change history", "Document information"),
        ("6.4.3.1", "Scope", "Purpose and scope"),
        ("6.4.3.2", "References", "References"),
        ("6.4.3.3", "Glossary", "Glossary"),
        ("6.4.4.1", "Summary of testing performed", "Summary, and the test environment under it"),
        ("6.4.4.2", "Deviations from planned testing", "Deviations and exclusions"),
        ("6.4.4.3", "Test completion evaluation", "The criteria under Summary"),
        ("6.4.4.4", "Factors that blocked progress", "Factors that blocked progress"),
        ("6.4.4.5", "Test measures", "Coverage, and Tests by area"),
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

  The per-test enumeration under The tests is not part of clause 6.4, which asks for a summary.
  It is included because it is the part readers most often want, and it belongs to the dynamic
  test process documents of the same standard.
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
    deviations: str = "",
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

{case_data(cases, described, registrations, descriptions)}
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
          #status-mark(case.status)
          #h(7pt) #text(fill: SLATE)[#calc.round(case.seconds, digits: 2) s]
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

#let total = cases.len()
#let count-of(wanted) = cases.filter(c => c.status == wanted).len()
#let seconds = cases.fold(0.0, (running, c) => running + c.seconds)

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
  This is the completion report for one run of Sen's test suite. It identifies the run, states
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
= Summary

{verdict}

// The counts as a table rather than as five large figures: the verdict above is what the section
// is for, and a row of 27pt numbers beside it competes for the same glance.
#block(above: 0.9em, below: 0.6em)[
  #table(
    columns: (1fr, 1fr, 1fr, 1fr, 1fr),
    align: (left, left, left, left, left),
    inset: (x: 4pt, y: 4pt),
    stroke: none,
    table.header(..column-labels(("Tests", "Passed", "Failed", "Skipped", "Duration"))),
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
== Test environment

#block(below: 1.3em, text(size: 8.5pt, fill: SLATE)[
  The item under test is Sen at the code revision above, built in the configuration above and run
  on the machine below. Figures from one configuration do not carry to another, and a duration
  means nothing without the processors it had; a second configuration is a second run of this
  report. What follows was read from the machine by the run itself, not stated by hand.
])

{environment}
{coverage}
== Tests by area

#block(below: 1em, text(size: 8.5pt, fill: SLATE)[
  Every test the run reported, grouped by the part of Sen it is declared in. A C++ case carries its
  description in the source beside it; a vitest or bats case is named with one; a test registered in
  CMake is given one there. Where a figure for covered lines appears it is that area's, so what is
  tested and how much of it is reached can be read on one line.
])

#table(
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
)


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

{deviations}
{parts["blockers"]}
{parts["risks"]}
{parts["deliverables"]}
{parts["assets"]}
{parts["lessons"]}
// One entry per test, not a table row: a description is prose and needs the width of the page,
// while the figures still line up on the right.
= The tests

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

{parts["conformance"]}
"""


def coverage_parts(report: Path | None, floor: str) -> tuple[str, dict[str, float], float | None]:
    """The coverage section, the figure for each area, and the line total, from one reading."""
    if report is None:
        return "", {}, None

    if not report.is_file():
        # Most legs do not instrument, so a missing report is ordinary.
        print(f"no coverage report at {report}; the document omits that section")
        return "", {}, None

    totals, files = read_coverage(report)
    if not totals:
        return "", {}, None

    by_area = {row.label: row.lines_cover for row in coverage_tree(files)}
    return coverage_section(totals, files, floor), by_area, float(totals["Lines"].rstrip("%"))


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

    if len(positional) < 2:
        print(
            "usage: render_test_document.py <report.xml> <out.typ> [configuration] [commit]"
            f" [sources] [{STOP_FLAG}] [{COVERAGE_FLAG}<coverage.txt>] [{FLOOR_FLAG}<percent>]"
            f" [{DESCRIPTIONS_FLAG}<test-descriptions.tsv>]"
            f" [{ENVIRONMENT_FLAG}<test-environment.tsv>] [{BUILD_FLAG}<build directory>]"
            f" [{LOGO_FLAG}<logo.svg>] [{ORGANIZATION_FLAG}<issuer>] [{APPROVER_FLAG}<name>]"
            f" [{REF_FLAG}<branch or tag>] [{RUN_FLAG}<build number>]",
            file=sys.stderr,
        )
        return 2

    report = Path(positional[0])
    out = Path(positional[1])
    configuration = positional[2] if len(positional) > 2 else "not stated"
    commit = positional[3] if len(positional) > 3 else "not stated"
    sources = Path(positional[4]) if len(positional) > 4 else None

    if not report.is_file():
        print(f"no test report at {report}", file=sys.stderr)
        return 1

    cases = read_cases(report)
    if not cases:
        print(EmptyReport(f"{report} holds no test cases"), file=sys.stderr)
        return 1

    recorded_environment = collected_environment(environment_file)

    started = run_started(report)
    # The cover says where this document came from, not what the software is called: a version
    # names a release, and which release a commit becomes is decided after the run that tested it.
    # It stays in the environment table below as the build property it is.
    meta = cover_identity(commit, ref, run, configuration, started, report)

    annotations = scan(sources) if sources is not None else {}
    registrations = read_registrations_for(sources, build)

    descriptions = collected_descriptions(descriptions_file)

    coverage, area_coverage, line_coverage = coverage_parts(coverage_report, coverage_floor)
    if coverage_report is not None and not coverage and coverage_report.is_file():
        print(f"{coverage_report} has no TOTAL row; the report format changed")
        return 1

    # Only a report that was read belongs in the references and the deliverables. The legs that
    # do not instrument are given a path to a file that is not there, and listing it would
    # document a measurement the document itself says was not taken.
    measured = coverage_report if coverage else None
    if measured is not None:
        meta.append(("Coverage report", f"{measured.name}  sha256 {file_digest(measured)}"))

    matched = [annotation_for(annotations, *split_name(case.classname, case.name)[:2]) for case in cases]
    unresolved = sum(
        1 for case, annotation in zip(cases, matched, strict=True) if not placement(case, annotation, registrations)[2]
    )
    verdict, criteria = conclusion_block(cases, line_coverage, coverage_floor, stopped_at_first_failure)
    deviations = deviations_section(cases, unresolved)
    environment = environment_section(recorded_environment)
    logo = place_logo(logo_file, out)
    dependency_set = next((value for name, value in recorded_environment if name == "Dependencies"), "not recorded")
    clause = {
        "information": document_information(organization, approver, commit),
        "references": references(report, measured, dependency_set),
        "glossary": glossary(),
        "blockers": blockers(),
        "risks": residual_risks(cases, line_coverage, coverage_floor, area_coverage),
        "deliverables": deliverables(report, measured, descriptions_file, environment_file),
        "assets": reusable_assets(),
        "lessons": lessons(),
        "conformance": conformance(approver),
    }

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
            deviations=deviations,
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
