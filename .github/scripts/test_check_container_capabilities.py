# === test_check_container_capabilities.py =============================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the reporting and the judging, which are deliberately different."""

import check_container_capabilities as cap

GRANTED = ("granted", "fine")
ABSENT = ("absent", "start it with the flag")
UNAVAILABLE = ("unavailable", "not a Linux container")


def probes(monkeypatch, **states):
    """Replaces the real probes, so a test says nothing about the machine running it."""
    monkeypatch.setattr(cap, "CAPABILITIES", {name: (lambda s=state: s) for name, state in states.items()})


def test_a_developer_who_can_grant_nothing_is_not_blocked(monkeypatch):
    """Degrading is the whole point.

    A workstation under someone else's administration still has to build and test.
    """
    probes(monkeypatch, rtprio=ABSENT, cpu_dma_latency=ABSENT)
    text, status = cap.report({})
    assert status == 0
    assert "absent" in text


def test_every_capability_is_named_whether_or_not_it_was_granted(monkeypatch):
    """An absent one that went unmentioned is how the fallback path passes for a feature."""
    probes(monkeypatch, rtprio=GRANTED, cpu_dma_latency=ABSENT)
    text, _ = cap.report({})
    assert "rtprio" in text
    assert "cpu_dma_latency" in text


def test_a_required_capability_that_is_missing_fails_and_says_which(monkeypatch):
    """What makes the grant worth having: it cannot lapse without a run going red."""
    probes(monkeypatch, rtprio=GRANTED, cpu_dma_latency=ABSENT)
    text, status = cap.report({"SEN_REQUIRE_CAPABILITIES": "rtprio,cpu_dma_latency"})
    assert status == 1
    assert "Required but not granted: cpu_dma_latency" in text


def test_requiring_only_what_is_granted_passes(monkeypatch):
    """A lane that asks for less than the machine offers is not an error."""
    probes(monkeypatch, rtprio=GRANTED, cpu_dma_latency=ABSENT)
    _, status = cap.report({"SEN_REQUIRE_CAPABILITIES": "rtprio"})
    assert status == 0


def test_a_platform_that_cannot_offer_one_still_fails_when_it_is_required(monkeypatch):
    """Unavailable is a kinder word than absent and means the same for the test.

    Reporting them differently helps a reader on Windows or macOS; treating them
    differently when the caller required one would excuse the thing being checked.
    """
    probes(monkeypatch, rtprio=UNAVAILABLE)
    _, status = cap.report({"SEN_REQUIRE_CAPABILITIES": "rtprio"})
    assert status == 1


def test_a_misspelled_requirement_fails_rather_than_requiring_nothing(monkeypatch):
    """A typo must not quietly require nothing.

    Dropping the unknown name would leave a lane that believes it is checking something
    and is not, which is the failure this file exists to prevent.
    """
    probes(monkeypatch, rtprio=GRANTED)
    text, status = cap.report({"SEN_REQUIRE_CAPABILITIES": "rtpriop"})
    assert status == 1
    assert "no such capability" in text


def test_the_required_list_takes_commas_or_spaces(monkeypatch):
    """Both get written, and a list parsed one way silently requires nothing the other way."""
    probes(monkeypatch, rtprio=ABSENT, cpu_dma_latency=ABSENT)
    for spelling in ("rtprio,cpu_dma_latency", "rtprio cpu_dma_latency", "rtprio, cpu_dma_latency"):
        assert cap.required({"SEN_REQUIRE_CAPABILITIES": spelling}) == ["rtprio", "cpu_dma_latency"], spelling


def test_no_requirement_set_requires_nothing(monkeypatch):
    """Reporting is the default: the gating lane opts in, the developer does not opt out."""
    assert cap.required({}) == []
    probes(monkeypatch, rtprio=ABSENT)
    assert cap.report({})[1] == 0


def test_the_real_probes_answer_without_raising():
    """They read the machine, so they must not throw on a platform lacking either.

    Windows has no resource module and macOS no RLIMIT_RTPRIO; both run this suite.
    """
    for name, probe in cap.CAPABILITIES.items():
        state, detail = probe()
        assert state in {"granted", "absent", "unavailable"}, name
        assert detail


def yama(monkeypatch, tmp_path, contents=None):
    """Points the probe at a file that either holds a value or does not exist."""
    path = tmp_path / "ptrace_scope"
    if contents is not None:
        path.write_text(contents, encoding="utf-8")
    monkeypatch.setattr(cap, "YAMA_PTRACE_SCOPE", str(path))


def test_yama_absent_is_not_reported_as_unrestricted(monkeypatch, tmp_path):
    """No file and a 0 permit the same tracing and are different facts.

    Collapsing them hides the difference between a kernel that cannot restrict tracing
    and one that has chosen not to, which is exactly what a container-versus-runner
    comparison is trying to see.
    """
    yama(monkeypatch, tmp_path)
    state, detail = cap.yama_ptrace_scope()
    assert state == "unavailable"
    assert "not loaded" in detail or "no Yama" in detail


def test_yama_zero_is_reported_as_granted(monkeypatch, tmp_path):
    """Loaded and unrestricted, which is a positive statement rather than an absence."""
    yama(monkeypatch, tmp_path, "0\n")
    assert cap.yama_ptrace_scope()[0] == "granted"


def test_yama_one_says_what_it_requires(monkeypatch, tmp_path):
    """The restricted value a runner usually carries, and the escape hatch it allows."""
    yama(monkeypatch, tmp_path, "1\n")
    state, detail = cap.yama_ptrace_scope()
    assert state == "restricted"
    assert "PR_SET_PTRACER" in detail


def test_an_unrecognised_yama_value_is_not_silently_accepted(monkeypatch, tmp_path):
    """A kernel that grows a fourth mode must not read as one of the three."""
    yama(monkeypatch, tmp_path, "9\n")
    state, detail = cap.yama_ptrace_scope()
    assert state == "restricted"
    assert "unrecognised" in detail


def test_the_ptrace_capability_is_read_from_the_effective_set(monkeypatch, tmp_path):
    """--cap-add only fills the bounding set, so asking for it is not holding it."""
    status = tmp_path / "status"
    status.write_text("Name:\tpython\nCapEff:\t0000000000080000\n", encoding="utf-8")
    monkeypatch.setattr(cap, "PROC_SELF_STATUS", str(status))
    assert cap.sys_ptrace() == ("granted", "CAP_SYS_PTRACE is held")


def test_an_empty_capability_set_is_reported_absent(monkeypatch, tmp_path):
    """The case every container we start is actually in."""
    status = tmp_path / "status"
    status.write_text("Name:\tpython\nCapEff:\t0000000000000000\n", encoding="utf-8")
    monkeypatch.setattr(cap, "PROC_SELF_STATUS", str(status))
    assert cap.sys_ptrace()[0] == "absent"
