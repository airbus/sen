# === generate_matrix_jobs.py ==========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""
Script to generate various forms of job matrices.
"""

import os
import json
import typing as tp
import argparse
from dataclasses import dataclass, is_dataclass, asdict


def convert_dataclass_to_json(obj) -> dict:
    """Converts the given dataclass to json."""
    if is_dataclass(obj):
        return {
            key: convert_dataclass_to_json(value) for key, value in asdict(obj).items()
        }

    return obj


@dataclass(frozen=True, order=True)
class Compiler:
    """Compiler specification"""

    name: str
    version: int
    cc: str
    cxx: str


@dataclass(frozen=True, order=True)
class Container:
    """Container specification"""

    image: str


@dataclass(frozen=True, order=True)
class JobSpecification:
    """Pipeline job specification that defines the configuration options."""

    name: str
    os: str
    runner: tp.Literal[
        "ubuntu-latest", "windows-2022", "self-hosted", "ubuntu-24.04-arm"
    ]
    container: Container | None
    compiler: Compiler
    std: tp.Literal[17, 20, 23]
    build_type: tp.Literal["Release", "Debug"]
    enable_coverage: bool = False

    def as_json(self) -> dict:
        """Converts the job spec into json."""
        return convert_dataclass_to_json(self)


@dataclass(frozen=True, order=True)
class JobSelector:
    """Selector specification that defines when to add a job."""

    job_spec: JobSpecification
    include_in_release_workflow: bool
    include_in_conan_workflow: bool
    include_in_standard_test_workflow: bool
    include_in_standard_test_workflow_also_main: bool


def compute_jobs(
    release: bool, conan: bool, standard_test: bool, target_main: bool
) -> list[JobSpecification]:
    """Computes the list of pipeline jobs that should run."""

    specified_jobs = [
        # Add gcc jobs
        JobSelector(
            job_spec=JobSpecification(
                "Basic GCC",
                "ubuntu-22.04",
                "self-hosted",
                None,
                Compiler("gcc", 12, "gcc-12", "g++-12"),
                17,
                "Debug",
            ),
            include_in_release_workflow=False,
            include_in_conan_workflow=True,
            include_in_standard_test_workflow=True,
            include_in_standard_test_workflow_also_main=False,
        ),
        JobSelector(
            job_spec=JobSpecification(
                "Basic GCC",
                "ubuntu-22.04",
                "self-hosted",
                None,
                Compiler("gcc", 12, "gcc-12", "g++-12"),
                17,
                "Release",
            ),
            include_in_release_workflow=True,
            include_in_conan_workflow=True,
            include_in_standard_test_workflow=True,
            include_in_standard_test_workflow_also_main=False,
        ),
        # Add clang jobs
        JobSelector(
            job_spec=JobSpecification(
                "Basic Clang",
                "ubuntu-24.04",
                "self-hosted",
                None,
                Compiler("clang", 20, "clang-20", "clang++-20"),
                17,
                "Debug",
                enable_coverage=True,
            ),
            include_in_release_workflow=False,
            include_in_conan_workflow=True,
            include_in_standard_test_workflow=True,
            include_in_standard_test_workflow_also_main=True,
        ),
        # Add msvc jobs
        JobSelector(
            job_spec=JobSpecification(
                "Basic Windows",
                "windows",
                "windows-2022",
                None,
                Compiler("msvc", 194, "cl", "cl"),
                17,
                "Release",
            ),
            include_in_release_workflow=True,
            include_in_conan_workflow=True,
            include_in_standard_test_workflow=True,
            include_in_standard_test_workflow_also_main=False,
        ),
        JobSelector(
            job_spec=JobSpecification(
                "Basic Windows",
                "windows",
                "windows-2022",
                None,
                Compiler("msvc", 194, "cl", "cl"),
                17,
                "Debug",
            ),
            include_in_release_workflow=False,
            include_in_conan_workflow=False,
            include_in_standard_test_workflow=True,
            include_in_standard_test_workflow_also_main=False,
        ),
        # Add amd64 jobs
        JobSelector(
            job_spec=JobSpecification(
                "Basic Ubuntu arm",
                "ubuntu-24.04",
                "ubuntu-24.04-arm",
                None,
                Compiler("gcc_arm64", 12, "gcc-14", "g++-14"),
                17,
                "Debug",
            ),
            include_in_release_workflow=False,
            include_in_conan_workflow=False,
            include_in_standard_test_workflow=True,
            include_in_standard_test_workflow_also_main=False,
        ),
    ]

    def include_job(job_selector: JobSelector) -> bool:
        if release:
            return job_selector.include_in_release_workflow

        if conan:
            return job_selector.include_in_conan_workflow

        if standard_test and target_main:
            return (
                job_selector.include_in_standard_test_workflow
                and job_selector.include_in_standard_test_workflow_also_main
            )

        if standard_test:
            return job_selector.include_in_standard_test_workflow

        print(
            f"Warning: could not correctly determine selection for job {job_selector}"
        )
        return False  # by default, we skip jobs

    return sorted(
        [
            job_selector.job_spec
            for job_selector in specified_jobs
            if include_job(job_selector)
        ]
    )


def generate_jobs_file(
    release: bool, conan: bool, standard_test: bool, target_main: bool
) -> None:
    """Generates the jobs file at GITHUB_OUTPUT."""
    jobs = compute_jobs(release, conan, standard_test, target_main)

    if output_file := os.environ.get("GITHUB_OUTPUT"):
        with open(output_file, "wt", encoding="utf-8") as matrix_file:
            matrix_file.write(f"jobs={json.dumps([j.as_json() for j in jobs])}")
    else:
        print("Error: No output file specified to write to.")


def main() -> None:
    """Runs the job matrix generator."""
    parser = argparse.ArgumentParser(
        prog="generate_matrix_jobs",
        description="Generates the list of required matrix jobs for various building needs.",
    )
    parser.add_argument(
        "--release",
        action=argparse.BooleanOptionalAction,
        help="Generate the jobs needed for building release artifacts.",
    )
    parser.add_argument(
        "--conan",
        action=argparse.BooleanOptionalAction,
        help="Generate the jobs needed to ensure all required conan packages work.",
    )
    parser.add_argument(
        "--standard-test",
        action=argparse.BooleanOptionalAction,
        help="Generate test jobs to ensure everything works correctly.",
    )
    parser.add_argument(
        "--target-main",
        action=argparse.BooleanOptionalAction,
        help="Specifies that we are explicitly building for the main branch.",
    )

    args = parser.parse_args()

    generate_jobs_file(
        release=args.release,
        conan=args.conan,
        standard_test=args.standard_test,
        target_main=args.target_main,
    )


if __name__ == "__main__":
    main()
