# === check_sanitizer_lane.py ==========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Proves a sanitizer lane can detect before its silence is allowed to mean anything.

A lane that reports nothing looks identical whether it found nothing or looked at nothing.
So before the suite runs: every binary must carry the runtime, every test must be told
where to write, there must still be a suite, and a deliberate finding must reach the
summary under the options a real test is given.

The canaries fault inside `sen::`-named code because every suppression this repository
writes is anchored on one; a canary faulting in `main` survives `race:sen::*` while the
whole suite goes quiet. The flags and the compiler come from the build for the same reason.

It does not prove that the log path a test carries is writable from where that test runs.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SUMMARISER = ROOT / ".github" / "scripts" / "summarise_sanitizer_reports.py"

# vptr is always recoverable -- -fno-sanitize-recover cannot promote it -- so unlike the
# rest of undefined behaviour its suppressions still apply on the gate. That makes it the
# one class a `vptr:sen::*` line can blind while the lane runs fail-fast.
VPTR_CANARY = """
    namespace sen::sanitizer_canary {
    struct Base {
      virtual ~Base() = default;
      virtual int kind() { return 0; }
    };
    struct Derived : Base {
      int extra = 1;
      int kind() override { return extra; }
    };
    int misuse() {
      Base plain;
      return reinterpret_cast<Derived *>(&plain)->kind();
    }
    }
    int main() {
      sen::sanitizer_canary::misuse();
      return 0;
    }
"""

# Each canary faults inside a sen::-named function, so a suppression written the way this
# repository writes them silences the canary too. In `main` it would survive `race:sen::*`
# and report a detection the suite can no longer make.
CANARIES = {
    "ThreadSanitizer": """
        #include <thread>
        namespace sen::sanitizer_canary {
        static long shared = 0;
        void bump() { for (int i = 0; i < 100000; ++i) ++shared; }
        }
        int main() {
          std::thread a(sen::sanitizer_canary::bump);
          std::thread b(sen::sanitizer_canary::bump);
          a.join();
          b.join();
          return 0;
        }
    """,
    "AddressSanitizer": """
        #include <cstdlib>
        namespace sen::sanitizer_canary {
        void write_past_the_end() {
          int *p = static_cast<int *>(malloc(sizeof(int) * 2));
          p[3] = 1;
          free(p);
        }
        }
        int main() {
          sen::sanitizer_canary::write_past_the_end();
          return 0;
        }
    """,
    "UndefinedBehaviorSanitizer": """
        #include <cstdio>
        namespace sen::sanitizer_canary {
        int overflow(int n) { return n + 1; }
        }
        int main() {
          std::printf("%d\\n", sen::sanitizer_canary::overflow(0x7fffffff));
          return 0;
        }
    """,
    "vptr": VPTR_CANARY,
    "deadlock": """
        #include <mutex>
        #include <thread>
        namespace sen::sanitizer_canary {
        std::mutex first;
        std::mutex second;
        void ab() { std::scoped_lock a(first); std::scoped_lock b(second); }
        void ba() { std::scoped_lock b(second); std::scoped_lock a(first); }
        }
        int main() {
          std::thread one(sen::sanitizer_canary::ab);
          one.join();
          std::thread two(sen::sanitizer_canary::ba);
          two.join();
          return 0;
        }
    """,
    "threadleak": """
        #include <pthread.h>
        namespace sen::sanitizer_canary {
        void *finish(void *) { return nullptr; }
        void leak_a_thread() {
          pthread_t id;
          pthread_create(&id, nullptr, finish, nullptr);
        }
        }
        int main() {
          sen::sanitizer_canary::leak_a_thread();
          return 0;
        }
    """,
    "signal": """
        #include <csignal>
        #include <cstdlib>
        namespace sen::sanitizer_canary {
        void handler(int) { free(malloc(64)); }
        void raise_it() {
          std::signal(SIGUSR1, handler);
          std::raise(SIGUSR1);
        }
        }
        int main() {
          sen::sanitizer_canary::raise_it();
          return 0;
        }
    """,
    # The allocation is what LeakSanitizer matches a suppression against, so it is the
    # part that has to sit in a sen:: frame.
    "LeakSanitizer": """
        #include <cstdlib>
        namespace sen::sanitizer_canary {
        void *leak() { return malloc(4096); }
        }
        int main() {
          volatile void *kept = sen::sanitizer_canary::leak();
          (void)kept;
          return 0;
        }
    """,
}

# Every deliberate finding a lane is held to, with the suppression kind that hides it. A
# tool appears more than once because undefined behaviour is a family. Address has no kind:
# its suppressions reach interceptors alone.
PROBES = (
    ("ThreadSanitizer", "ThreadSanitizer", "TSAN_OPTIONS", "race"),
    ("ThreadSanitizer", "deadlock", "TSAN_OPTIONS", "deadlock"),
    ("ThreadSanitizer", "threadleak", "TSAN_OPTIONS", "thread"),
    ("ThreadSanitizer", "signal", "TSAN_OPTIONS", "signal"),
    ("AddressSanitizer", "AddressSanitizer", "ASAN_OPTIONS", None),
    ("UndefinedBehaviorSanitizer", "UndefinedBehaviorSanitizer", "UBSAN_OPTIONS", "signed-integer-overflow"),
    ("UndefinedBehaviorSanitizer", "vptr", "UBSAN_OPTIONS", "vptr"),
    ("LeakSanitizer", "LeakSanitizer", "LSAN_OPTIONS", "leak"),
)

# Which tools each lane must be shown to detect, and the runtime symbols its binaries
# carry. LeakSanitizer rides with the address build and is the one whose suppression file
# is edited most, so it is checked rather than assumed.
LANES = {
    "address": {
        "flags": ["-fsanitize=address,undefined"],
        "tools": ["AddressSanitizer", "UndefinedBehaviorSanitizer", "LeakSanitizer"],
    },
    "thread": {
        "flags": ["-fsanitize=thread"],
        "tools": ["ThreadSanitizer"],
    },
}

# What a binary carries once a sanitizer was compiled in, not merely linked against. A
# library compiled clean and linked with -fsanitize still shows __asan_report, so that
# symbol proves nothing; these appear only with instrumentation, gcc as well as clang.
RUNTIME_SYMBOLS = {"address": "__asan_init", "undefined": "__ubsan_handle", "thread": "__tsan_init"}

# ctest's environment-modification vocabulary. Only `set` states the whole value; the
# others edit something inherited that cannot be reconstructed from here, so they are
# named in the failure rather than silently misread as a value.
COMPLETE_OPERATIONS = ("set",)
PARTIAL_OPERATIONS = (
    "unset",
    "reset",
    "string_append",
    "string_prepend",
    "path_list_append",
    "path_list_prepend",
    "cmake_list_append",
    "cmake_list_prepend",
)

OPTIONS = re.compile(r"^([A-Z]+SAN_OPTIONS)=(.*)$")
OPERATION = re.compile(rf"^({'|'.join(COMPLETE_OPERATIONS + PARTIAL_OPERATIONS)}):(.*)$", re.S)
LOG_PATH = re.compile(r"(^|:)log_path=[^:]+")
SUPPRESSIONS = re.compile(r"(^|:)suppressions=([^:]+)")

# Below this the suite has lost a whole test binary rather than a few tests: the largest
# one carries about four fifths of the 1589, so losing it leaves under three hundred.
# Ordinary growth and pruning stay well clear. Raise it when the suite does.
MINIMUM_TESTS = 1000

# Named so a missing tool cannot be excused as "the lane does not claim it".
MISSING_SHOWN = 5


def run_tool(command: list[str], **arguments) -> subprocess.CompletedProcess:
    """Runs one of the tools this check needs, naming it if it is not installed.

    nm comes from binutils, which nothing here installs -- it is on a runner only because
    gcc is. Absent, any of these ends the step in a traceback rather than a verdict.
    """
    try:
        return subprocess.run(command, capture_output=True, text=True, check=False, **arguments)
    except FileNotFoundError as error:
        raise RuntimeError(f"{command[0]} is not installed where this check runs") from error


def build_flags(build_dir: Path, lane: dict) -> tuple[list[str], list[str]]:
    """The sanitizer flags the build actually used, rather than the ones declared here.

    A build that narrowed its own set still satisfies a canary compiled with the wider one,
    so the canary would stand in for a lane that no longer exists.
    """
    database = build_dir / "compile_commands.json"
    if not database.exists():
        return lane["flags"], []
    used: set[tuple[str, ...]] = set()
    for entry in json.loads(database.read_text(encoding="utf-8")):
        arguments = entry.get("arguments") or entry.get("command", "").split()
        # By prefix, not by the word "sanitize" anywhere: this repository compiles with
        # -DGIT_REF_SPEC=refs/heads/<branch>, and a branch named for this work matched.
        flags = tuple(a for a in arguments if a.startswith(("-fsanitize", "-fno-sanitize")))
        if flags:
            used.add(flags)
    if not used:
        return lane["flags"], []
    first = sorted(used)[0]
    if len(used) == 1:
        return list(first), []
    return list(first), [
        f"the build does not use one sanitizer flag set throughout -- {len(used)} distinct sets, "
        "so a canary compiled with one of them does not stand in for the rest"
    ]


def required_symbols(flags: list[str]) -> list[str]:
    """The runtime symbols the flags in use should have put into every binary."""
    named: set[str] = set()
    for flag in flags:
        if flag.startswith("-fsanitize="):
            named.update(flag.split("=", 1)[1].split(","))
    return sorted({RUNTIME_SYMBOLS[name] for name in named if name in RUNTIME_SYMBOLS})


def build_compiler(build_dir: Path, fallback: str) -> str:
    """The compiler the build used, read from its cache."""
    cache = build_dir / "CMakeCache.txt"
    if cache.exists():
        for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("CMAKE_CXX_COMPILER:"):
                _, _, value = line.partition("=")
                if value.strip():
                    return value.strip()
    return fallback


def linked_here(build_dir: Path) -> list[str]:
    """What this build links, read from the generator's own edges.

    File names cannot answer this: Sen versions its libraries, so the real files are
    `libcore.so.0.0.0` and `sen-0.0.0` with a symlink beside them. A vendored dependency is
    not linked here at all, so the edges exclude it without a list to maintain.
    """
    ninja = build_dir / "build.ninja"
    if not ninja.exists():
        return []
    edge = re.compile(r"^build ([^:\n]+): (?:CXX|C)_(?:SHARED_LIBRARY|EXECUTABLE|MODULE_LIBRARY)_LINKER__\S+", re.M)
    outputs: set[str] = set()
    for match in edge.finditer(ninja.read_text(encoding="utf-8", errors="replace")):
        outputs.update(part.replace("$ ", " ") for part in match.group(1).split())
    return sorted(outputs)


def binaries(build_dir: Path) -> list[Path]:
    """Everything the lane linked that the runtime should be in, and that exists.

    A configuration that leaves a target out -- benchmarks here -- names it and does not
    produce it, which is not this check's business.
    """
    return [build_dir / output for output in linked_here(build_dir) if (build_dir / output).is_file()]


def options_of(test: dict) -> tuple[dict[str, str], list[str], str | None]:
    """The sanitizer options one test is given, what cannot be read, and its mount if any."""
    properties = {entry["name"]: entry["value"] for entry in test.get("properties", [])}
    found: dict[str, str] = {}
    unreadable: list[str] = []
    mount: str | None = None
    for key in ("ENVIRONMENT", "ENVIRONMENT_MODIFICATION"):
        for entry in properties.get(key, []):
            if entry.startswith("SEN_INTEGRATION_TEST_MOUNT="):
                _, _, value = entry.partition("=")
                operation = OPERATION.match(value)
                # Any of ctest's operations, not only set:. Read literally, the operation
                # became part of the path and every container test was then called stranded.
                mount = operation.group(2) if operation else value
            match = OPTIONS.match(entry)
            if not match:
                continue
            name, value = match.group(1), match.group(2)
            operation = OPERATION.match(value) if key == "ENVIRONMENT_MODIFICATION" else None
            if operation is None:
                found[name] = value
            elif operation.group(1) in COMPLETE_OPERATIONS:
                found[name] = operation.group(2)
            else:
                unreadable.append(f"{test['name']} sets {name} with {operation.group(1)}:")
    return found, unreadable, mount


def registered_options(
    build_dir: Path,
) -> tuple[dict[str, dict[str, str]], list[str], set[str], dict[str, str], dict[str, dict[str, str]]]:
    """The sanitizer options ctest hands each test, read from ctest rather than assumed.

    Both registration paths land here: gtest_discover_tests writes ENVIRONMENT, the
    integration suites write ENVIRONMENT_MODIFICATION with an operation on each value.
    """
    listing = run_tool(["ctest", "--show-only=json-v1"], cwd=build_dir)
    if listing.returncode != 0 or not listing.stdout.strip():
        raise RuntimeError(f"ctest could not list the tests in {build_dir}: {listing.stderr.strip()[:300]}")

    tests: dict[str, dict[str, str]] = {}
    uninterpretable: list[str] = []
    running: set[str] = set()
    parked: dict[str, dict[str, str]] = {}
    mounts: dict[str, str] = {}
    for test in json.loads(listing.stdout)["tests"]:
        properties = {entry["name"]: entry["value"] for entry in test.get("properties", [])}
        if properties.get("DISABLED"):
            # Parking one is ordinary and must not fail a lane. Kept apart rather than
            # counted: what matters is whether parking took a whole option set with it.
            parked[test["name"]] = options_of(test)[0]
            continue
        if test.get("command"):
            running.add(Path(test["command"][0]).name)
        found, unreadable, mount = options_of(test)
        uninterpretable.extend(unreadable)
        tests[test["name"]] = found
        if mount:
            mounts[test["name"]] = mount
    return tests, sorted(set(uninterpretable)), running, mounts, parked


def writing_into_a_container(tests: dict[str, dict[str, str]], mounts: dict[str, str]) -> list[str]:
    """Tests told to write their findings somewhere that goes away with the container.

    Left on the job's path, the sanitizer creates it inside the container, writes the
    finding there, and cleanup deletes it -- and a log path silences stderr, so the
    container log holds nothing either.
    """
    stranded = []
    for name, mount in mounts.items():
        options = tests.get(name, {})
        root = Path(os.path.normpath(mount))
        paths = [match.group(0).split("=", 1)[1] for value in options.values() for match in LOG_PATH.finditer(value)]
        # normpath first, and by component: a bare prefix passes /home/builder/sentinel
        # against /home/builder/sen, and passes anything that climbs back out with "..".
        if paths and not all(Path(os.path.normpath(path)).is_relative_to(root) for path in paths):
            stranded.append(name)
    return sorted(stranded)


def option_sets(tests: dict[str, dict[str, str]]) -> set[tuple]:
    """The distinct ways a test is configured, as a canary sees them.

    Collapsed on the log path, which detects() redirects anyway: counting two tests apart
    for writing to different places would run the canary once per test.
    """
    return {
        tuple(sorted((name, LOG_PATH.sub(r"\1log_path=SAME", value)) for name, value in options.items()))
        for options in tests.values()
        if options
    }


def silent_tests(tests: dict[str, dict[str, str]]) -> list[str]:
    """Tests with nowhere to write, which report a finding to nobody."""
    return sorted(name for name, options in tests.items() if not any("log_path=" in v for v in options.values()))


def resembles_sen(tool: str, kind: str, variable: str, canary: str, flags: list[str], compiler: str) -> str | None:
    """Whether a suppression in this repository's own style silences the canary.

    The wildcard run comes first: a check whose suppressions this build ignores cannot be
    used to blind the lane, and demanding silence from it would fail an honest one. It is
    run rather than inferred from flags, because `vptr` stays suppressible under
    -fno-sanitize-recover where the rest of undefined behaviour does not.
    """
    with tempfile.TemporaryDirectory() as directory:
        workspace = Path(directory)
        source = workspace / "canary.cpp"
        source.write_text(canary, encoding="utf-8")
        binary = workspace / "canary"
        build = run_tool([compiler, *flags, "-g", "-o", str(binary), str(source)])
        if build.returncode != 0:
            return f"the {tool} {kind} canary would not compile: {build.stderr.strip()[:200]}"

        def reports_under(pattern: str | None) -> bool:
            reports = workspace / f"reports-{pattern or 'none'}".replace(":", "_").replace("*", "star")
            reports.mkdir(exist_ok=True)
            # This probe asks a question about names, so symbolize is pinned: without
            # symbols `sen::*` matches nothing while a bare `*` still matches the module,
            # and every canary would look as though it did not resemble Sen.
            options = f"symbolize=1:log_path={reports}/report"
            if pattern is not None:
                suppressions = workspace / "suppressions.txt"
                suppressions.write_text(f"{pattern}\n", encoding="utf-8")
                options = f"suppressions={suppressions}:{options}"
            ambient = {name: value for name, value in os.environ.items() if not name.endswith("SYMBOLIZER_PATH")}
            run = subprocess.run(
                [str(binary)], env={**ambient, variable: options}, capture_output=True, text=True, check=False
            )
            written = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in reports.iterdir())
            everything = f"{written}\n{run.stdout}\n{run.stderr}"
            return (tool in everything or "runtime error:" in everything) and "uppressions" not in everything

        if not reports_under(f"{kind}:*"):
            # Suppressions bite here, so one written the way this repository writes them
            # has to reach the canary as well as the code it stands in for.
            if reports_under(f"{kind}:sen::*"):
                return (
                    f"the {tool} {kind} canary still reports under `{kind}:sen::*`, so it does not fault "
                    "inside a sen:: frame: a suppression that silences the suite would leave this check green"
                )
        return None


def this_side_of_the_mount(value: str) -> str:
    """Points a suppressions= at the copy of that file the check can actually open.

    The container suites name theirs by the path inside the runtime image, which the canary
    cannot reach. It is the same file, so taking it from the checkout keeps it in play --
    dropping it would make a broadened entry invisible.
    """

    def replace(match: re.Match) -> str:
        named = Path(match.group(2))
        if named.exists():
            return match.group(0)
        local = ROOT / "cmake" / "util" / named.name
        return f"{match.group(1)}suppressions={local}" if local.exists() else match.group(0)

    return SUPPRESSIONS.sub(replace, value)


def detects(
    tool: str, canary: str, kind: str | None, flags: list[str], compiler: str, environment: dict[str, str]
) -> str | None:
    """Runs a deliberate finding under the given options, returning what went wrong.

    The options are a real test's, with log_path redirected so the lane's own report
    directory is not polluted with a finding nobody made.
    """
    with tempfile.TemporaryDirectory() as directory:
        workspace = Path(directory)
        source = workspace / "canary.cpp"
        source.write_text(CANARIES[canary], encoding="utf-8")
        binary = workspace / "canary"
        reports = workspace / "reports"
        reports.mkdir()

        build = run_tool([compiler, *flags, "-g", "-o", str(binary), str(source)])
        if build.returncode != 0:
            return f"the canary would not compile with {' '.join(flags)}: {build.stderr.strip()[:200]}"

        # Overlaid rather than replacing: ctest applies a test's ENVIRONMENT per variable,
        # so what the job exports reaches the test. A job-level detect_leaks=0 silences most
        # of the suite, and a canary in a bare environment would not notice.
        redirected = {**os.environ} | {
            name: this_side_of_the_mount(LOG_PATH.sub(f"\\1log_path={reports}/report", value))
            for name, value in environment.items()
        }
        # The canary is meant to fail, so its exit status says nothing; the report does.
        try:
            subprocess.run([str(binary)], env=redirected, capture_output=True, check=False)
        except OSError as error:
            raise RuntimeError(f"the {tool} canary could not be run from {binary.parent}: {error.strerror}") from error
        if not any(reports.iterdir()):
            # Which suppression file, if one is what silenced it. The thread sanitizer writes
            # nothing at all when it suppresses, so without this every kind reports the same
            # sentence and names neither itself nor the file to look in.
            without = {name: SUPPRESSIONS.sub("", value) for name, value in redirected.items()}
            subprocess.run([str(binary)], env={**os.environ, **without}, capture_output=True, check=False)
            named = sorted(name for name in redirected if name.endswith("SAN_OPTIONS"))
            what = f"{tool} {kind}" if kind else tool
            if any(reports.iterdir()):
                files = sorted({m.group(2) for v in environment.values() for m in SUPPRESSIONS.finditer(v)})
                return f"a suppression silences the {what} canary; the file in play is {', '.join(files) or 'unknown'}"
            return f"the {what} canary wrote no report under {named}, so a finding would not reach the summary"

        summary = subprocess.run(
            [sys.executable, str(SUMMARISER), str(reports)], capture_output=True, text=True, check=False
        ).stdout
        if tool not in summary:
            # A suppressed run still writes a file, holding the tally and no finding. Saying
            # so points at the suppression file rather than at the summariser.
            written = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in reports.iterdir())
            if "failed to parse suppressions" in written:
                return f"{tool} could not parse the suppression file its options name: {written.strip()[:200]}"
            if "failed to read suppressions file" in written:
                # A distinct cause with its own fix, and it used to arrive as a summariser fault.
                return f"{tool} could not read the suppression file its options name: {written.strip()[:160]}"
            if "Suppressions used" in written:
                return f"{tool} found the canary and a suppression silenced it, so a real finding would be silenced too"
            return f"{tool} wrote a report the summariser did not attribute to it"
        return None


def instrumentation_problems(build_dir: Path, symbols: list[str]) -> list[str]:
    """Whether everything the lane built carries the runtime it compiled against.

    Every binary, not one: a flag that stopped reaching some targets leaves the rest
    instrumented, and one instrumented library is enough to hide the whole of the rest.
    """
    found = binaries(build_dir)
    if not found:
        return [f"no libraries or executables under {build_dir}, so nothing can be checked for instrumentation"]

    bare = []
    for path in found:
        listing = run_tool(["nm", "-D", str(path)])
        if listing.returncode != 0:
            continue  # not an object nm can read; the ones that matter all are
        # Every one of them: link flags alone put a report symbol into an object that was
        # compiled clean, so any() would accept a library nothing instrumented.
        if not all(symbol in listing.stdout for symbol in symbols):
            bare.append(path.name)

    if not bare:
        return []
    shown = ", ".join(sorted(bare)[:MISSING_SHOWN])
    more = f" and {len(bare) - MISSING_SHOWN} more" if len(bare) > MISSING_SHOWN else ""
    return [
        f"{len(bare)} of {len(found)} binaries do not carry all of {symbols}, "
        f"so they were not instrumented: {shown}{more}"
    ]


def suites_that_vanished(build_dir: Path, running: set[str]) -> list[str]:
    """Suites cmake registered that ctest then runs nothing from.

    A count cannot see this: one suite carries four fifths of the tests, so the rest can be
    emptied and the total still clears any floor. The denominator is cmake's own discovery
    include, which names the executable whatever the target is called.
    """
    executable = re.compile(r"TEST_EXECUTABLE \[==\[(.*?)\]==\]", re.S)
    suites = set()
    for include in build_dir.rglob("*_include.cmake"):
        found = executable.search(include.read_text(encoding="utf-8", errors="replace"))
        if found and Path(found.group(1)).is_file():
            suites.add(Path(found.group(1)).name)
    silent = sorted(suites - running)
    if not silent:
        return []
    return [f"{len(silent)} test binaries were built and contribute no tests: {', '.join(sorted(silent))}"]


def reporting_problems(
    tests: dict[str, dict[str, str]],
    uninterpretable: list[str],
    vanished: list[str] | None = None,
    mounts: dict[str, str] | None = None,
    parked: dict[str, dict[str, str]] | None = None,
) -> list[str]:
    """Whether there is a suite, and whether every test in it has somewhere to write."""
    problems = list(vanished or [])

    # A count cannot see this: switching off every integration and smoke test moves the
    # total by under two percent while halving the ways a test is configured, and a
    # suppression need only reach one of those to silence a lane.
    lost = option_sets(parked or {}) - option_sets(tests)
    if lost:
        problems.append(
            f"{len(lost)} way(s) of configuring a test survive only on tests that are DISABLED, "
            "so nothing running exercises them"
        )
    if len(tests) < MINIMUM_TESTS:
        problems.append(
            f"ctest lists {len(tests)} tests, below the {MINIMUM_TESTS} floor: a whole test binary "
            "is missing rather than a few tests, and the lane would report on what is left"
        )
    if uninterpretable:
        shown = "; ".join(uninterpretable[:MISSING_SHOWN])
        problems.append(f"this check cannot read the options {len(uninterpretable)} test(s) are given: {shown}")

    stranded = writing_into_a_container(tests, mounts or {})
    if stranded:
        shown = ", ".join(stranded[:MISSING_SHOWN])
        more = f" and {len(stranded) - MISSING_SHOWN} more" if len(stranded) > MISSING_SHOWN else ""
        problems.append(
            f"{len(stranded)} test(s) write their findings inside a container that is then deleted, "
            f"because their log path is not under the mount they are given: {shown}{more}"
        )

    absent = sorted(name for name in tests if name.endswith("_NOT_BUILT"))
    if absent:
        problems.append(f"{len(absent)} test executable(s) were never built: {', '.join(absent[:MISSING_SHOWN])}")
    silent = [name for name in silent_tests(tests) if not name.endswith("_NOT_BUILT")]
    if silent:
        shown = ", ".join(silent[:MISSING_SHOWN])
        more = f" and {len(silent) - MISSING_SHOWN} more" if len(silent) > MISSING_SHOWN else ""
        problems.append(
            f"{len(silent)} of {len(tests)} tests carry no log_path, so a finding there reaches nobody: {shown}{more}"
        )
    return problems


def detection_problems(lane: dict, flags: list[str], compiler: str, tests: dict[str, dict[str, str]]) -> list[str]:
    """Whether a deliberate finding reaches the summary under each set of options in use.

    One canary per distinct set: the suites are registered by two paths, and a
    suppression change need only reach one of them to silence a lane.
    """
    # Collapsed on the log path, which detects() redirects anyway: two tests differing only
    # in where they write are one set to a canary, and counting them apart would run it once
    # per test rather than once per way a test is configured.
    distinct = option_sets(tests)
    if not distinct:
        return ["no test carries sanitizer options at all, so nothing could report a finding"]

    problems = []
    for tool, canary, variable, kind in PROBES:
        if tool not in lane["tools"]:
            continue
        if kind is not None:
            problem = resembles_sen(tool, kind, variable, CANARIES[canary], flags, compiler)
            if problem:
                problems.append(problem)
        for options in sorted(distinct):
            problem = detects(tool, canary, kind, flags, compiler, dict(options))
            if problem:
                problems.append(problem)
    return problems


def main() -> int:
    """Checks the lane is instrumented, tells every test where to write, and detects."""
    parser = argparse.ArgumentParser(
        prog="check_sanitizer_lane",
        description="Fails unless the lane's sanitizers are shown to detect and report.",
    )
    parser.add_argument("build_dir", help="the lane's build folder, holding bin/")
    parser.add_argument("sanitizer", choices=sorted(LANES), help="the sanitizer option the lane built with")
    parser.add_argument("--compiler", default=None, help="compiler for the canaries; the build's own by default")
    arguments = parser.parse_args()

    build_dir = Path(arguments.build_dir)
    lane = LANES[arguments.sanitizer]
    flags, flag_problems = build_flags(build_dir, lane)
    compiler = arguments.compiler or build_compiler(build_dir, "clang++-20")

    try:
        tests, uninterpretable, running, mounts, parked = registered_options(build_dir)
        problems = [
            *flag_problems,
            *instrumentation_problems(build_dir, required_symbols(flags) or ["__asan_init"]),
            *reporting_problems(tests, uninterpretable, suites_that_vanished(build_dir, running), mounts, parked),
            *detection_problems(lane, flags, compiler, tests),
        ]
    except (RuntimeError, json.JSONDecodeError) as error:
        print(f"This lane cannot be checked: {error}", file=sys.stderr)
        return 1

    if problems:
        print(f"This lane cannot be shown to detect {arguments.sanitizer} findings:", file=sys.stderr)
        for problem in dict.fromkeys(problems):
            print(f"  {problem}", file=sys.stderr)
        return 1

    distinct = len(option_sets(tests))
    print(
        f"lane checked: {len(tests)} tests carry a log path, {' '.join(flags)} reached every binary, "
        f"and a deliberate finding reached the summary under {distinct} option set(s) "
        f"for {', '.join(lane['tools'])}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
