# === test_generate_matrix_jobs.py =====================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the exact job matrix emitted for every workflow flag combination.

When the matrix changes, the expectations here change in the same commit.
"""

import pytest
from generate_matrix_jobs import SPECIFIED_JOBS, Compiler, JobSpecification, compute_jobs

GCC_DEBUG = ("Basic GCC", "gcc-12", "Debug", "ubuntu-22.04", "x86")
GCC_RELEASE = ("Basic GCC", "gcc-12", "Release", "ubuntu-22.04", "x86")
CLANG_COVERAGE = ("Basic Clang", "clang-20", "Debug", "ubuntu-24.04", "x86")
MSVC_RELEASE = ("Basic Windows", "cl", "Release", "windows-2022", "x86")
ARM_DEBUG = ("Basic Ubuntu arm", "gcc-12", "Debug", "ubuntu-24.04-arm", "arm")


def job_keys(jobs: list[JobSpecification]) -> list[tuple[str, str, str, str, str]]:
    """Reduces job specs to the fields that identify a matrix leg."""
    return [(job.name, job.compiler.cc, job.build_type, job.runner, job.arch) for job in jobs]


def test_standard_test_job_set():
    """Standard tests run the three Linux x86 legs plus the arm leg."""
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    assert job_keys(jobs) == [CLANG_COVERAGE, GCC_DEBUG, GCC_RELEASE, ARM_DEBUG]


def test_standard_test_main_job_set():
    """Post-merge main builds run the coverage leg and the shipping leg."""
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=True)
    assert job_keys(jobs) == [CLANG_COVERAGE, GCC_RELEASE]


def test_conan_job_set():
    """Packaging runs the Linux legs plus the MSVC Release leg."""
    jobs = compute_jobs(release=False, conan=True, standard_test=False, target_main=False)
    assert job_keys(jobs) == [CLANG_COVERAGE, GCC_DEBUG, GCC_RELEASE, MSVC_RELEASE]


def test_release_job_set():
    """Releases build gcc and MSVC, Release only."""
    jobs = compute_jobs(release=True, conan=False, standard_test=False, target_main=False)
    assert job_keys(jobs) == [GCC_RELEASE, MSVC_RELEASE]


def test_only_coverage_leg_enables_coverage():
    """Exactly one leg collects coverage."""
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    assert [job.name for job in jobs if job.enable_coverage] == ["Basic Clang"]


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
            os="ubuntu-22.04",
            runner="self-hosted",  # the retired datacenter runner's label
            container=None,
            compiler=Compiler(name="gcc", version=12, cc="gcc-12", cxx="g++-12"),
            arch="x86",
            std=17,
            build_type="Debug",
        )


def test_standard_test_legs_build_examples():
    """Every standard-test leg builds the examples."""
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    assert all(job.enable_examples for job in jobs)


def test_windows_legs_keep_the_defaults():
    """The Windows legs do not build the examples."""
    jobs = compute_jobs(release=True, conan=False, standard_test=False, target_main=False)
    windows = [job for job in jobs if job.os == "windows"]
    assert windows
    assert all(not job.enable_examples for job in windows)


def test_pull_request_packaging_job_set():
    """A pull request packages the shipping configuration only."""
    jobs = compute_jobs(release=False, conan=True, standard_test=False, target_main=False, pull_request=True)
    assert job_keys(jobs) == [GCC_RELEASE]


def test_only_the_shipping_leg_checks_the_package():
    """The CPack archive is built and checked once, on the shipping leg."""
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    checked = [job for job in jobs if job.check_package]
    assert job_keys(checked) == [GCC_RELEASE]


def test_standard_test_specs_in_full():
    """Pins every field of every standard-test leg, not just the identifying ones.

    The fields left out of job_keys are load-bearing: os feeds the cache keys
    and cxx is exported as the compiler.
    """
    jobs = compute_jobs(release=False, conan=False, standard_test=True, target_main=False)
    assert [job.as_json() for job in jobs] == [
        {
            "name": "Basic Clang",
            "os": "ubuntu-24.04",
            "runner": "ubuntu-24.04",
            "container": None,
            "compiler": {"name": "clang", "version": 20, "cc": "clang-20", "cxx": "clang++-20"},
            "arch": "x86",
            "std": 17,
            "build_type": "Debug",
            "enable_coverage": True,
            "enable_examples": True,
            "check_package": False,
        },
        {
            "name": "Basic GCC",
            "os": "ubuntu-22.04",
            "runner": "ubuntu-22.04",
            "container": None,
            "compiler": {"name": "gcc", "version": 12, "cc": "gcc-12", "cxx": "g++-12"},
            "arch": "x86",
            "std": 17,
            "build_type": "Debug",
            "enable_coverage": False,
            "enable_examples": True,
            "check_package": False,
        },
        {
            "name": "Basic GCC",
            "os": "ubuntu-22.04",
            "runner": "ubuntu-22.04",
            "container": None,
            "compiler": {"name": "gcc", "version": 12, "cc": "gcc-12", "cxx": "g++-12"},
            "arch": "x86",
            "std": 17,
            "build_type": "Release",
            "enable_coverage": False,
            "enable_examples": True,
            "check_package": True,
        },
        {
            "name": "Basic Ubuntu arm",
            "os": "ubuntu-24.04",
            "runner": "ubuntu-24.04-arm",
            "container": None,
            "compiler": {"name": "gcc", "version": 12, "cc": "gcc-12", "cxx": "g++-12"},
            "arch": "arm",
            "std": 17,
            "build_type": "Debug",
            "enable_coverage": False,
            "enable_examples": True,
            "check_package": False,
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
