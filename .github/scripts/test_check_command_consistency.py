# === test_check_command_consistency.py ================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins what counts as a broken command in prose, and what does not.

The "accepted" cases are the ones that matter. A page has to be able to explain
why `--profile` is the wrong flag without the check firing on the explanation,
or the pages that teach the rule would be the ones that fail it.
"""

import sys
from pathlib import Path

import check_command_consistency as check

PROFILE_RULE = check.RULES[0]


def rejects(line):
    """True when the profile rule reports this line."""
    return check.check_line(PROFILE_RULE, line)


# --------------------------------------------------------------------------------------------------------------------
# Rejected: invocations a reader would copy
# --------------------------------------------------------------------------------------------------------------------


def test_rejects_bare_profile_on_install():
    """The invocation F-229 was about: it is the one a reader copies first."""
    assert rejects("conan install . --profile=sen_gcc_x86 --build=missing")


def test_rejects_bare_profile_on_build():
    """`conan build` takes the same flag and rots the same way."""
    assert rejects("conan build . --profile=sen_gcc_x86")


def test_rejects_bare_profile_with_a_space():
    """`--profile x` and `--profile=x` are the same command and must fail alike."""
    assert rejects("conan install . --profile sen_gcc --build=missing")


def test_rejects_bare_profile_on_create():
    """Package authors copy `create` lines; the flag is wrong there too."""
    assert rejects("conan create . --profile=sen_msvc")


# --------------------------------------------------------------------------------------------------------------------
# Accepted: correct invocations
# --------------------------------------------------------------------------------------------------------------------


def test_accepts_profile_all():
    """The correction the rule exists to steer people towards."""
    assert not rejects("conan install . --profile:all=sen_gcc --build=missing")


def test_accepts_explicit_host_and_build():
    """Naming both profiles is equally correct and must not be flagged."""
    assert not rejects("conan install . --profile:host=sen_gcc --profile:build=sen_gcc")


# --------------------------------------------------------------------------------------------------------------------
# Accepted: prose about the flag, which is how the rule gets taught
# --------------------------------------------------------------------------------------------------------------------


def test_accepts_prose_explaining_the_flag():
    """A page must be able to say why the flag is wrong without failing on it."""
    line = "`--profile:all` sets both the host and the build profile. `--profile` sets only the host one,"
    assert not rejects(line)


def test_accepts_prose_naming_the_flag_without_an_invocation():
    """Naming a flag mid-sentence is not an invocation."""
    assert not rejects("and Conan then looks for a build profile named `default` when you pass `--profile`.")


def test_accepts_an_unrelated_tool_taking_a_profile():
    """Other tools have a `--profile` and none of this applies to them."""
    assert not rejects("aws s3 ls --profile=default")


# --------------------------------------------------------------------------------------------------------------------
# The cross-file version check
# --------------------------------------------------------------------------------------------------------------------


def test_collects_a_pinned_installer_version(tmp_path, monkeypatch):
    """A version is only comparable across files once it is found in one."""
    page = tmp_path / "install.md"
    page.write_text("curl -sSf https://example/install.sh | sh -s -- 0.6.0\n")
    monkeypatch.setattr(check, "REPO_ROOT", tmp_path)
    assert check.collect_versions([page]) == {"0.6.0": ["install.md:1"]}


def test_disagreeing_versions_are_grouped(tmp_path, monkeypatch):
    """Two files pinning different versions is the defect; both must be reported."""
    one = tmp_path / "a.md"
    one.write_text("curl -sSf https://example/install.sh | sh -s -- 0.6.0\n")
    two = tmp_path / "b.md"
    two.write_text("curl -sSf https://example/install.sh | sh -s -- 0.5.2\n")
    monkeypatch.setattr(check, "REPO_ROOT", tmp_path)
    found = check.collect_versions([one, two])
    assert sorted(found) == ["0.5.2", "0.6.0"]


# --------------------------------------------------------------------------------------------------------------------
# Path handling. The checker is invoked with relative paths as often as absolute ones,
# and the guard for that originally lived in only one of the two entry points, so the
# other raised `ValueError: not in the subpath of` on any relative argument.
# --------------------------------------------------------------------------------------------------------------------


def test_display_path_accepts_a_relative_path():
    """pre-commit passes relative paths, so this is the common case."""
    assert check.display_path(Path("docs/getting_started/install.md")) == "docs/getting_started/install.md"


def test_display_path_trims_the_repository_root_from_an_absolute_path():
    """The absolute form, which the entry point that walks the tree produces."""
    assert check.display_path(check.REPO_ROOT / "docs" / "index.md") == "docs/index.md"


def test_runs_against_a_relative_filename_without_raising(tmp_path, monkeypatch):
    """The whole point: this is the invocation that used to crash."""
    page = tmp_path / "guide.md"
    page.write_text("curl -sSf https://example/install.sh | sh -s -- 0.6.0\n")
    monkeypatch.chdir(tmp_path)
    monkeypatch.setattr(sys, "argv", ["check_command_consistency.py", "guide.md"])
    assert check.main() == 0


def test_reports_a_relative_filename_as_given(tmp_path, monkeypatch, capsys):
    """What a reader sees in the failure: the path they recognise, not an absolute one."""
    page = tmp_path / "guide.md"
    page.write_text("conan install . --profile=sen_gcc --build=missing\n")
    monkeypatch.chdir(tmp_path)
    monkeypatch.setattr(sys, "argv", ["check_command_consistency.py", "guide.md"])
    assert check.main() == 1
    assert "guide.md:1" in capsys.readouterr().out


def test_a_path_outside_the_repository_is_reported_not_crashed_on():
    """It reports paths. One it cannot make relative must still be reportable."""
    assert check.display_path(Path("/tmp/elsewhere/README.md")) == "/tmp/elsewhere/README.md"
    assert check.display_path(Path("docs/guide.md")) == "docs/guide.md"
