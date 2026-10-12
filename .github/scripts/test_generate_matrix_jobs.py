# === test_generate_matrix_jobs.py =====================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the exact job matrix emitted for every workflow flag combination.

When the matrix changes, the expectations here change in the same commit.
"""

from collections import defaultdict

import pytest
from generate_matrix_jobs import SPECIFIED_JOBS, Compiler, JobSpecification, compute_jobs

GCC_DEBUG = ("Basic GCC", "gcc-12", "Debug", "ubuntu-22.04", "x86")
GCC_RELEASE = ("Basic GCC", "gcc-12", "Release", "ubuntu-22.04", "x86")
CLANG_DEBUG = ("Basic Clang", "clang-20", "Debug", "ubuntu-24.04", "x86")
MSVC_RELEASE = ("Basic Windows", "cl", "Release", "windows-2022", "x86")
GCC_RELWITHDEBINFO = ("Basic GCC (debug information)", "gcc-12", "RelWithDebInfo", "ubuntu-22.04", "x86")
GCC_DEBUG_BUILD = ("Basic GCC (debug build)", "gcc-12", "Debug", "ubuntu-22.04", "x86")
MSVC_RELWITHDEBINFO = ("Basic Windows (debug information)", "cl", "RelWithDebInfo", "windows-2022", "x86")


def job_keys(jobs: list[JobSpecification]) -> list[tuple[str, str, str, str, str]]:
    """Reduces job specs to the fields that identify a matrix leg."""
    return [(job.name, job.compiler.cc, job.build_type, job.runner, job.arch) for job in jobs]


def test_standard_test_job_set():
    """The three Linux x86 legs and both Windows ones, which is what a release publishes."""
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    assert job_keys(jobs) == [
        GCC_DEBUG,
        GCC_RELEASE,
        GCC_RELWITHDEBINFO,
        MSVC_RELEASE,
        MSVC_RELWITHDEBINFO,
    ]


def test_main_runs_what_pull_requests_run():
    """A push to main runs the same legs as a pull request.

    A pull request is tested against main as it was. What lands is that change
    combined with whatever main became since, and only a run on main sees that
    combination. The merge queue would test it too, but a merge that bypasses
    the queue leaves no other lane that does.
    """
    on_pull_request = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    on_main = compute_jobs(release=False, conan=False, standard_test=True, target_main=True)
    assert job_keys(on_main) == job_keys(on_pull_request)


def test_every_shipped_configuration_is_tested():
    """A release publishes nothing the standard-test matrix has not run.

    The document a release delivers is rendered by the run that tested the commit, and the
    release workflow only builds and packages -- it runs no tests of its own. So a shipped
    configuration missing from the matrix below is both untested and invisible in the report,
    which is how a shipped configuration goes untested.
    """

    def built(job):
        # Compared on what decides the program, not on the leg's name: the release selects some
        # configurations through their own entry, so names differ where the build does not. The
        # runner is in the key because it sets the libc the binary is built against.
        return (job.compiler.cc, job.arch, job.build_type, job.std, job.runner)

    shipped = compute_jobs(release=True, conan=False, standard_test=False, target_main=False)
    tested = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    missing = sorted({built(job) for job in shipped} - {built(job) for job in tested})
    assert not missing, f"shipped but never tested: {missing}"


def test_a_pull_request_builds_only_what_a_release_ships():
    """The converse of the test above, so the two together hold the matrix equal to the release.

    A pull request does not build a configuration no release
    publishes. A leg outside that set spends a merge's wall clock and a slice of a 10 GiB cache
    budget on a binary nobody receives, and the clang and arm legs between them held 57 per cent
    of all compiler cache while shipping nothing. Both are covered nightly instead.

    Coverage instrumentation is not part of the comparison: the measuring leg's binaries are
    instrumented and unshippable, but its configuration is one a release builds, which is what
    test_one_leg_measures_coverage_and_it_is_a_build_we_ship already requires.
    """

    def built(job):
        return (job.compiler.cc, job.arch, job.build_type, job.std, job.runner)

    shipped = compute_jobs(release=True, conan=False, standard_test=False, target_main=False)
    tested = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    extra = sorted({built(job) for job in tested} - {built(job) for job in shipped})
    assert not extra, f"tested on every pull request but shipped by no release: {extra}"


def test_conan_job_set():
    """Packaging runs the Linux legs plus the MSVC Release leg."""
    jobs = compute_jobs(release=False, conan=True, standard_test=False, target_main=False)
    assert job_keys(jobs) == [CLANG_DEBUG, GCC_DEBUG, GCC_RELEASE, MSVC_RELEASE]


def test_release_job_set():
    """What ships, what carries debug information, and an unoptimised build on Linux.

    Windows has no unoptimised leg: dependencies stay Release by design, so a Debug Sen
    there would link the release runtime and be useless for the one thing it is for.
    """
    jobs = compute_jobs(release=True, conan=False, standard_test=False, target_main=False)
    assert job_keys(jobs) == [
        GCC_RELEASE,
        GCC_DEBUG_BUILD,
        GCC_RELWITHDEBINFO,
        MSVC_RELEASE,
        MSVC_RELWITHDEBINFO,
    ]


def test_a_release_and_its_debug_information_share_a_toolchain():
    """Every leg of one release, so one may not build in the image and another not.

    A second toolchain would add differences on top of the build type that already separates
    them.
    """
    jobs = compute_jobs(release=True, conan=False, standard_test=False, target_main=False)
    by_compiler = defaultdict(set)
    for job in jobs:
        by_compiler[job.compiler.name].add(job.ci_image)
    assert {name: sorted(images) for name, images in by_compiler.items()} == {
        "gcc": ["22.04"],
        "msvc": [""],
    }


def test_one_leg_measures_coverage_and_it_is_a_build_we_ship():
    """Coverage comes from a single configuration, and it has to be one a release publishes.

    Optimisation moves which line a hit is counted against, so the leg is a debug build. A leg
    outside what a release ships would describe a binary nobody receives.
    """

    def built(job):
        return (job.compiler.cc, job.arch, job.build_type, job.runner)

    tested = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    measuring = [job for job in tested if job.enable_coverage]
    assert len(measuring) == 1, [job.name for job in measuring]

    leg = measuring[0]
    assert leg.build_type == "Debug", leg.build_type
    shipped = compute_jobs(release=True, conan=False, standard_test=False, target_main=False)
    assert built(leg) in {built(job) for job in shipped}, f"{built(leg)} is not published by a release"


def test_no_flag_selection_fails_loudly():
    """compute_jobs raises when no workflow flag is set."""
    with pytest.raises(ValueError, match="could not determine"):
        compute_jobs(release=False, conan=False, standard_test=False, target_main=False)


def test_positional_construction_is_rejected():
    """Constructing a spec positionally raises TypeError."""
    with pytest.raises(TypeError):
        Compiler("gcc", 12, "gcc-12", "g++-12")


def test_invalid_literal_value_is_rejected():
    """Values outside a Literal field fail at construction time."""
    with pytest.raises(ValueError, match="runner="):
        JobSpecification(
            name="Bad",
            runner="self-hosted",  # the retired datacenter runner's label
            compiler=Compiler(name="gcc", version=12, cc="gcc-12", cxx="g++-12"),
            arch="x86",
            std=17,
            build_type="Debug",
        )


def test_standard_test_legs_build_examples():
    """Every standard-test leg builds the examples."""
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    assert all(job.enable_examples for job in jobs)


def test_container_tests_run_on_the_x86_gcc_legs():
    """The container-based suite runs on the x86 gcc legs, on a matching base."""
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    with_containers = {job.build_type: job for job in jobs if job.runtime_base}
    assert set(with_containers) == {"Debug", "Release"}
    for job in with_containers.values():
        assert job.compiler.name == "gcc"
        assert job.arch == "x86"
        # the runner label and the docker base name the same Ubuntu release
        assert job.runtime_base.replace(":", "-") == job.runner


def test_release_legs_build_examples():
    """Every release leg builds the examples, Windows included."""
    jobs = compute_jobs(release=True, conan=False, standard_test=False, target_main=False)
    assert [job for job in jobs if job.compiler.name == "msvc"]
    assert all(job.enable_examples for job in jobs)


def test_pull_request_packaging_job_set():
    """A pull request packages the shipping configuration on each operating system."""
    jobs = compute_jobs(release=False, conan=True, standard_test=False, target_main=False, pull_request=True)
    assert job_keys(jobs) == [GCC_RELEASE, MSVC_RELEASE]


def test_a_pull_request_packages_on_every_operating_system():
    """Every operating system the packaging lane runs on main also runs on a pull request.

    An operating system decides the archive, the install tree and the flags a consumer
    compiles with. One reached only after a merge cannot refuse the change that breaks it.
    """
    on_pull_request = compute_jobs(release=False, conan=True, standard_test=False, target_main=False, pull_request=True)
    on_main = compute_jobs(release=False, conan=True, standard_test=False, target_main=False)
    systems = {job.runner.split("-")[0] for job in on_main}
    assert systems <= {job.runner.split("-")[0] for job in on_pull_request}


def test_each_operating_system_checks_its_package():
    """The CPack archive is built and checked on every leg that ships one.

    Windows ships a different archive, so the Linux leg cannot stand in for it.
    """
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    checked = [job for job in jobs if job.check_package]
    assert job_keys(checked) == [GCC_RELEASE, GCC_RELWITHDEBINFO, MSVC_RELEASE, MSVC_RELWITHDEBINFO]
    # The list above is exact, so a leg added on one operating system and not the other would
    # pass unnoticed if nothing named the property the list is there to hold.
    assert {job.runner for job in checked} == {"ubuntu-22.04", "windows-2022"}


def test_standard_test_specs_in_full():
    """Pins every field of every standard-test leg, not just the identifying ones.

    The fields left out of job_keys are load-bearing: cxx is exported as the
    compiler, and runtime_base registers the container suite.
    """
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    assert [job.as_json() for job in jobs] == [
        {
            "name": "Basic GCC",
            "runner": "ubuntu-22.04",
            "compiler": {"name": "gcc", "version": 12, "cc": "gcc-12", "cxx": "g++-12"},
            "arch": "x86",
            "std": 17,
            "build_type": "Debug",
            "enable_coverage": True,
            "enable_examples": True,
            "runtime_base": "ubuntu:22.04",
            "check_package": False,
            "ci_image": "22.04",
        },
        {
            "name": "Basic GCC",
            "runner": "ubuntu-22.04",
            "compiler": {"name": "gcc", "version": 12, "cc": "gcc-12", "cxx": "g++-12"},
            "arch": "x86",
            "std": 17,
            "build_type": "Release",
            "enable_coverage": False,
            "enable_examples": True,
            "runtime_base": "ubuntu:22.04",
            "check_package": True,
            "ci_image": "22.04",
        },
        {
            "name": "Basic GCC (debug information)",
            "runner": "ubuntu-22.04",
            "compiler": {"name": "gcc", "version": 12, "cc": "gcc-12", "cxx": "g++-12"},
            "arch": "x86",
            "std": 17,
            "build_type": "RelWithDebInfo",
            "enable_coverage": False,
            "enable_examples": True,
            "runtime_base": "",
            "check_package": True,
            "ci_image": "22.04",
        },
        {
            "name": "Basic Windows",
            "runner": "windows-2022",
            "compiler": {"name": "msvc", "version": 194, "cc": "cl", "cxx": "cl"},
            "arch": "x86",
            "std": 17,
            "build_type": "Release",
            "enable_coverage": False,
            "enable_examples": True,
            "runtime_base": "",
            "check_package": True,
            "ci_image": "",
        },
        {
            "name": "Basic Windows (debug information)",
            "runner": "windows-2022",
            "compiler": {"name": "msvc", "version": 194, "cc": "cl", "cxx": "cl"},
            "arch": "x86",
            "std": 17,
            "build_type": "RelWithDebInfo",
            "enable_coverage": False,
            "enable_examples": True,
            "runtime_base": "",
            "check_package": True,
            "ci_image": "",
        },
    ]


def test_every_declared_leg_is_reachable():
    """A leg no workflow can select advertises coverage that never happens."""
    selections = [
        compute_jobs(release=True, conan=False, standard_test=False, target_main=False),
        compute_jobs(release=False, conan=True, standard_test=False, target_main=False),
        compute_jobs(release=False, conan=True, standard_test=False, target_main=False, pull_request=True),
        compute_jobs(release=False, conan=False, standard_test=True, target_main=False),
        compute_jobs(release=False, conan=False, standard_test=True, target_main=True),
    ]
    reachable = {job for jobs in selections for job in jobs}
    declared = {selector.job_spec for selector in SPECIFIED_JOBS}
    assert declared == reachable
