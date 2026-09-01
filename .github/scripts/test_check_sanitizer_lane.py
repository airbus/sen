# === test_check_sanitizer_lane.py =====================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Keeps the lane check from being emptied by a rename or a parsing slip.

The check earns its keep only while every tool it claims to cover has something to
trigger it, while that something still looks like the code the suppressions are written
against, and while it can still read the options ctest hands a test. A tool dropped from
one table and left in another, a canary that drifts back into `main`, or a shape ctest
emits that the parser silently ignores would make the lane pass by testing less. That is
the failure the check itself exists to prevent, one level up.
"""

import json
import re

import check_sanitizer_lane as check


def test_every_tool_a_lane_claims_has_a_canary():
    """Without this, dropping a canary would quietly shrink what a lane proves."""
    for sanitizer, lane in check.LANES.items():
        assert lane["tools"], f"{sanitizer} claims no tools"
        for tool in lane["tools"]:
            assert tool in check.CANARIES, f"{sanitizer} claims {tool} with no canary to trigger it"


def test_the_address_lane_claims_the_leak_sanitizer():
    """It rides with the address build and its suppression file is the one people edit.

    Left out, a single `leak:sen::*` silences every leak in the product and the check
    goes on saying a deliberate finding reached the summary.
    """
    assert "LeakSanitizer" in check.LANES["address"]["tools"]


def test_the_sanitizer_names_are_the_ones_the_recipe_offers():
    """The argument a lane passes is a conan option value; a rename there must fail here."""
    recipe = (check.ROOT / "conanfile.py").read_text(encoding="utf-8")
    offered = re.search(r'"sanitizer":\s*\[([^\]]*)\]', recipe)
    assert offered, "conanfile.py no longer declares a sanitizer option"
    values = {value.strip().strip('"') for value in offered.group(1).split(",")}
    assert set(check.LANES) <= values, f"{set(check.LANES) - values} is not offered by the recipe"


def test_each_canary_triggers_the_tool_it_is_for():
    """A canary that stopped provoking anything would leave the lane passing over nothing."""
    provocations = {
        "ThreadSanitizer": "thread",
        "AddressSanitizer": "malloc",
        "UndefinedBehaviorSanitizer": "0x7fffffff",
        "LeakSanitizer": "malloc",
        "vptr": "reinterpret_cast",
        "deadlock": "scoped_lock",
        "threadleak": "pthread_create",
        "signal": "std::raise",
    }
    for tool, source in check.CANARIES.items():
        assert provocations[tool] in source, f"the {tool} canary no longer contains what provokes it"


def test_every_probe_names_a_canary_and_a_lane_that_wants_it():
    """A probe with no canary, or for a tool no lane claims, is a check that never runs."""
    claimed = {tool for lane in check.LANES.values() for tool in lane["tools"]}
    for tool, canary, variable, _ in check.PROBES:
        assert canary in check.CANARIES, f"{tool} probes with {canary}, which has no canary"
        assert tool in claimed, f"{tool} is probed and no lane claims it"
        assert variable.endswith("SAN_OPTIONS")


def test_undefined_behaviour_is_probed_for_more_than_one_check():
    """It is a family, and a suppression naming one of its checks says nothing about the rest.

    vptr is the one that matters: it is always recoverable, so -fno-sanitize-recover cannot
    promote it and its suppressions still apply on the gate, where every other undefined
    check's are ignored. A single overflow probe would leave `vptr:sen::*` blinding the lane.
    """
    kinds = {kind for tool, _, _, kind in check.PROBES if tool == "UndefinedBehaviorSanitizer"}
    assert "vptr" in kinds, "the one undefined-behaviour class fail-fast does not protect is unprobed"
    assert len(kinds) > 1


def test_the_address_sanitizer_is_probed_without_a_suppression_kind():
    """Its suppressions reach interceptors alone, so no name pattern would apply to it.

    Established by trying: no shape of interceptor_via_fun, interceptor_via_lib,
    interceptor_name or odr_violation silenced a heap overflow.
    """
    kinds = {kind for tool, _, _, kind in check.PROBES if tool == "AddressSanitizer"}
    assert kinds == {None}


def test_an_emptied_suite_is_caught_even_though_the_count_stays_high(tmp_path):
    """The largest suite carries four fifths of the tests, so a count cannot see this.

    Every other unit suite can be emptied and the total still clears any floor. What catches
    it is cmake's discovery include, which names the executable whatever the target is called.
    """
    for name in ("core_test", "util_checks"):
        binary = tmp_path / name
        binary.write_text("", encoding="utf-8")
        (tmp_path / f"{name}[1]_include.cmake").write_text(
            f"gtest_discover_tests_impl(\n  TEST_EXECUTABLE [==[{binary}]==]\n)\n", encoding="utf-8"
        )

    assert check.suites_that_vanished(tmp_path, {"core_test", "util_checks"}) == []
    problems = check.suites_that_vanished(tmp_path, {"core_test"})
    assert any("util_checks" in problem for problem in problems), problems


def test_a_suite_whose_executable_was_never_built_is_not_blamed_on_the_suite(tmp_path):
    """The include file names it whether or not the build produced it.

    ctest registers a `_NOT_BUILT` placeholder in that case, which is reported separately;
    counting the absent executable here as well would say the same thing twice.
    """
    (tmp_path / "gone_test[1]_include.cmake").write_text(
        f"gtest_discover_tests_impl(\n  TEST_EXECUTABLE [==[{tmp_path / 'gone_test'}]==]\n)\n", encoding="utf-8"
    )
    assert check.suites_that_vanished(tmp_path, set()) == []


def test_an_unreadable_option_is_reported_as_that_and_not_as_a_blind_lane():
    """Naming the operation sends a reader to the registration, not to the sanitizer."""
    whole = {f"test_{index}": {"ASAN_OPTIONS": "log_path=/r/report"} for index in range(1589)}
    problems = check.reporting_problems(whole, ["some_test sets ASAN_OPTIONS with string_append:"])
    assert any("cannot read the options" in problem for problem in problems), problems


def test_redirecting_the_log_path_keeps_the_rest_of_the_options():
    """The canary must run under the lane's suppressions, or it proves only itself."""
    redirected = check.LOG_PATH.sub("\\1log_path=/tmp/x/report", "suppressions=/real/tsan.txt:log_path=/r/report")
    assert redirected == "suppressions=/real/tsan.txt:log_path=/tmp/x/report"


def test_a_suppression_path_from_inside_the_container_is_found_in_the_checkout():
    """The container suites name their suppressions by the path the runtime image sees.

    The canary cannot open that path. Resolving it to the same file in the checkout keeps
    the suppression in play; dropping it would make a broadened entry invisible.
    """
    mounted = "suppressions=/home/builder/sen/cmake/util/asan_ignorelist.txt:log_path=/r/report"
    resolved = check.this_side_of_the_mount(mounted)
    assert str(check.ROOT / "cmake" / "util" / "asan_ignorelist.txt") in resolved
    assert resolved.endswith(":log_path=/r/report"), "the rest of the options must survive"

    real = f"suppressions={check.ROOT / 'cmake' / 'util' / 'lsan_ignorelist.txt'}:log_path=/r/report"
    assert check.this_side_of_the_mount(real) == real, "a path that exists is left alone"

    nowhere = "suppressions=/nowhere/at/all.txt:log_path=/r/report"
    assert check.this_side_of_the_mount(nowhere) == nowhere, "an unresolvable path is reported, not invented"


def test_the_runtime_symbols_follow_the_flags_the_build_used():
    """The symbols asked for must be the ones instrumentation actually leaves behind.

    A build compiled clean and merely linked with -fsanitize still exports a report symbol.
    Wrong either way fails an honest lane or passes a blind one.
    """
    assert check.required_symbols(["-fsanitize=address,undefined"]) == ["__asan_init", "__ubsan_handle"]
    assert check.required_symbols(["-fsanitize=address"]) == ["__asan_init"]
    assert check.required_symbols(["-fsanitize=thread"]) == ["__tsan_init"]
    assert "__asan_report" not in check.RUNTIME_SYMBOLS.values()
    assert check.required_symbols(["-O2"]) == []


def test_a_branch_name_is_not_a_sanitizer_flag(tmp_path):
    """A flag is recognised by its prefix, not by the word "sanitize" anywhere in it.

    This repository compiles with -DGIT_REF_SPEC=refs/heads/<branch>.
    """
    database = tmp_path / "compile_commands.json"
    database.write_text(
        json.dumps([{"command": '/usr/bin/clang++ -DGIT_REF_SPEC="refs/heads/ci/sanitizer-canary" -c a.cpp'}]),
        encoding="utf-8",
    )
    flags, problems = check.build_flags(tmp_path, check.LANES["address"])
    assert flags == check.LANES["address"]["flags"], "a branch name must not be read as a flag"
    assert problems == []


def test_a_build_with_more_than_one_flag_set_is_reported_rather_than_sampled(tmp_path):
    """One canary compiled with one target's flags does not stand in for a different one."""
    (tmp_path / "compile_commands.json").write_text(
        json.dumps(
            [
                {"command": "clang++ -fsanitize=address,undefined -c a.cpp"},
                {"command": "clang++ -fsanitize=address,undefined -fno-sanitize=signed-integer-overflow -c b.cpp"},
            ]
        ),
        encoding="utf-8",
    )
    _, problems = check.build_flags(tmp_path, check.LANES["address"])
    assert any("one sanitizer flag set" in problem for problem in problems), problems


def test_a_container_suite_told_to_write_outside_its_mount_is_caught():
    """Its findings are written inside a container and deleted with it.

    The log path has to be rewritten to the mount alongside the suppression paths. Both are
    in the same options, so this is answerable from the listing without running anything.
    """
    mounts = {"in_a_container": "/home/builder/sen"}
    stranded = {"in_a_container": {"ASAN_OPTIONS": "suppressions=/home/builder/sen/x.txt:log_path=/workspace/r/report"}}
    assert check.writing_into_a_container(stranded, mounts) == ["in_a_container"]

    landed = {
        "in_a_container": {"ASAN_OPTIONS": "suppressions=/home/builder/sen/x.txt:log_path=/home/builder/sen/r/report"}
    }
    assert check.writing_into_a_container(landed, mounts) == []

    ordinary = {"on_the_job": {"ASAN_OPTIONS": "log_path=/workspace/r/report"}}
    assert check.writing_into_a_container(ordinary, {}) == [], "a test with no mount is not in this business"


def test_every_thread_suppression_kind_the_ignorelist_uses_is_probed():
    """A suppression kind with no canary silences its whole class unnoticed.

    Broadening one `deadlock:` entry to the namespace hides every lock-order inversion in
    the product, and a canary for `race:` alone would not notice.
    """
    ignorelist = (check.ROOT / "cmake" / "util" / "tsan_ignorelist.txt").read_text(encoding="utf-8")
    used = {line.split(":", 1)[0] for line in ignorelist.splitlines() if ":" in line and not line.startswith("#")}
    probed = {kind for tool, _, _, kind in check.PROBES if tool == "ThreadSanitizer"}
    assert used <= probed, f"{sorted(used - probed)} appear in the ignorelist and nothing probes them"


def test_parking_tests_matters_when_it_takes_a_whole_option_set_with_it():
    """Not how many are parked -- how many ways of configuring a test survive.

    Switching off every integration and smoke test barely moves the total while halving the
    option sets, and a suppression need only reach one of those to silence a lane.
    """
    live = {f"t{index}": {"ASAN_OPTIONS": "log_path=/r/report"} for index in range(1560)}
    lost = {"integration": {"ASAN_OPTIONS": "suppressions=/x:fast_unwind_on_malloc=0:log_path=/r/report"}}
    assert any(
        "survive only on tests that are DISABLED" in p for p in check.reporting_problems(live, [], None, None, lost)
    )

    kept = {"another": {"ASAN_OPTIONS": "log_path=/elsewhere/report"}}
    assert check.reporting_problems(live, [], None, None, kept) == [], "a shape that survives is not a problem"


def test_two_tests_differing_only_in_where_they_write_are_one_option_set():
    """The canary redirects the log path, so counting them apart runs it once per test."""
    tests = {
        "a": {"ASAN_OPTIONS": "suppressions=/x:log_path=/r/a/report"},
        "b": {"ASAN_OPTIONS": "suppressions=/x:log_path=/r/b/report"},
        "c": {"ASAN_OPTIONS": "suppressions=/other:log_path=/r/c/report"},
    }
    assert len(check.option_sets(tests)) == 2
