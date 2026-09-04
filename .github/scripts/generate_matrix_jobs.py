# === generate_matrix_jobs.py ==========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Script to generate various forms of job matrices."""

import argparse
import json
import os
import typing as tp
from dataclasses import asdict, dataclass, fields


@dataclass(frozen=True, order=True, kw_only=True)
class Compiler:
    """Compiler specification."""

    name: str
    version: int
    cc: str
    cxx: str


@dataclass(frozen=True, order=True, kw_only=True)
class Container:
    """Container specification."""

    image: str


@dataclass(frozen=True, order=True, kw_only=True)
class JobSpecification:
    """Pipeline job specification that defines the configuration options."""

    name: str
    os: str
    runner: tp.Literal["ubuntu-latest", "ubuntu-22.04", "ubuntu-24.04", "windows-2022", "ubuntu-24.04-arm"]
    container: Container | None
    compiler: Compiler
    arch: tp.Literal["x86", "arm"]
    std: tp.Literal[17, 20, 23]
    build_type: tp.Literal["Release", "Debug"]
    enable_coverage: bool = False
    enable_examples: bool = False
    # Docker base image for the container-based integration tests. A non-empty
    # value registers them, and the base must match the runner's OS so the
    # binaries mounted into the containers find a matching runtime. Empty on
    # the legs that would only add container startups without covering
    # anything the x86 gcc legs do not already cover.
    runtime_base: str = ""
    # Builds the CPack archive and checks its contents. Set on the shipping
    # configuration only: the archive is the same everywhere it is built.
    check_package: bool = False

    def __post_init__(self):
        """Validates every Literal-typed field against its allowed values."""
        for field in fields(self):
            if tp.get_origin(field.type) is tp.Literal:
                value = getattr(self, field.name)
                allowed = tp.get_args(field.type)
                if value not in allowed:
                    raise ValueError(f"{field.name}={value!r} is not one of {allowed}")

    def as_json(self) -> dict:
        """Converts the job spec into json."""
        return asdict(self)


@dataclass(frozen=True, order=True, kw_only=True)
class JobSelector:
    """Selector specification that defines when to add a job."""

    job_spec: JobSpecification
    include_in_release_workflow: bool
    include_in_conan_workflow: bool
    include_in_conan_workflow_on_pull_requests: bool
    include_in_standard_test_workflow: bool
    include_in_standard_test_workflow_also_main: bool


# Every leg the pipeline knows about. A leg no workflow can select advertises
# coverage that never happens, so test_generate_matrix_jobs.py asserts that
# every entry here is selected by some workflow.
SPECIFIED_JOBS = [
    # Add gcc jobs
    JobSelector(
        job_spec=JobSpecification(
            name="Basic GCC",
            os="ubuntu-22.04",
            runner="ubuntu-22.04",
            container=None,
            compiler=Compiler(name="gcc", version=12, cc="gcc-12", cxx="g++-12"),
            arch="x86",
            std=17,
            build_type="Debug",
            enable_examples=True,
            runtime_base="ubuntu:22.04",
        ),
        include_in_release_workflow=False,
        include_in_conan_workflow=True,
        include_in_conan_workflow_on_pull_requests=False,
        include_in_standard_test_workflow=True,
        include_in_standard_test_workflow_also_main=False,
    ),
    JobSelector(
        job_spec=JobSpecification(
            name="Basic GCC",
            os="ubuntu-22.04",
            runner="ubuntu-22.04",
            container=None,
            compiler=Compiler(name="gcc", version=12, cc="gcc-12", cxx="g++-12"),
            arch="x86",
            std=17,
            build_type="Release",
            enable_examples=True,
            runtime_base="ubuntu:22.04",
            check_package=True,
        ),
        include_in_release_workflow=True,
        include_in_conan_workflow=True,
        include_in_conan_workflow_on_pull_requests=True,
        include_in_standard_test_workflow=True,
        # Also after a merge: this leg carries the package-archive check.
        include_in_standard_test_workflow_also_main=True,
    ),
    # Add clang jobs
    JobSelector(
        job_spec=JobSpecification(
            name="Basic Clang",
            os="ubuntu-24.04",
            runner="ubuntu-24.04",
            container=None,
            compiler=Compiler(name="clang", version=20, cc="clang-20", cxx="clang++-20"),
            arch="x86",
            std=17,
            build_type="Debug",
            enable_coverage=True,
            enable_examples=True,
        ),
        include_in_release_workflow=False,
        include_in_conan_workflow=True,
        include_in_conan_workflow_on_pull_requests=False,
        include_in_standard_test_workflow=True,
        include_in_standard_test_workflow_also_main=True,
    ),
    # Add msvc jobs
    JobSelector(
        job_spec=JobSpecification(
            name="Basic Windows",
            os="windows",
            runner="windows-2022",
            container=None,
            compiler=Compiler(name="msvc", version=194, cc="cl", cxx="cl"),
            arch="x86",
            std=17,
            build_type="Release",
        ),
        include_in_release_workflow=True,
        include_in_conan_workflow=True,
        include_in_conan_workflow_on_pull_requests=False,
        # Examples stay off until the suite is green here (SEN-1725).
        include_in_standard_test_workflow=True,
        include_in_standard_test_workflow_also_main=False,
    ),
    # Add arm jobs
    JobSelector(
        job_spec=JobSpecification(
            name="Basic Ubuntu arm",
            os="ubuntu-24.04",
            runner="ubuntu-24.04-arm",
            container=None,
            compiler=Compiler(name="gcc", version=12, cc="gcc-12", cxx="g++-12"),
            arch="arm",
            std=17,
            build_type="Debug",
            enable_examples=True,
        ),
        include_in_release_workflow=False,
        include_in_conan_workflow=False,
        include_in_conan_workflow_on_pull_requests=False,
        include_in_standard_test_workflow=True,
        include_in_standard_test_workflow_also_main=False,
    ),
]


def compute_jobs(
    release: bool, conan: bool, standard_test: bool, target_main: bool, pull_request: bool = False
) -> list[JobSpecification]:
    """Computes the list of pipeline jobs that should run."""

    def include_job(job_selector: JobSelector) -> bool:
        if release:
            return job_selector.include_in_release_workflow

        if conan and pull_request:
            return job_selector.include_in_conan_workflow_on_pull_requests

        if conan:
            return job_selector.include_in_conan_workflow

        if standard_test and target_main:
            return (
                job_selector.include_in_standard_test_workflow
                and job_selector.include_in_standard_test_workflow_also_main
            )

        if standard_test:
            return job_selector.include_in_standard_test_workflow

        raise ValueError(f"could not determine the workflow selection for job {job_selector.job_spec.name}")

    jobs = [job_selector.job_spec for job_selector in SPECIFIED_JOBS if include_job(job_selector)]
    if not jobs:
        raise ValueError("the selection produced no jobs; a matrix job would silently not exist")

    # Explicit key: ordering by the dataclass would compare container against None.
    return sorted(jobs, key=lambda job: (job.name, job.runner, job.build_type, job.std))


def generate_jobs_file(
    release: bool, conan: bool, standard_test: bool, target_main: bool, pull_request: bool = False
) -> None:
    """Generates the jobs file at GITHUB_OUTPUT."""
    jobs = compute_jobs(release, conan, standard_test, target_main, pull_request)

    output_file = os.environ.get("GITHUB_OUTPUT")
    if not output_file:
        raise SystemExit("Error: No output file specified to write to.")

    with open(output_file, "a", encoding="utf-8") as matrix_file:
        matrix_file.write(f"jobs={json.dumps([j.as_json() for j in jobs])}\n")


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
    parser.add_argument(
        "--pull-request",
        action=argparse.BooleanOptionalAction,
        help="Generate only the packaging jobs that run on a pull request.",
    )

    args = parser.parse_args()

    if sum(bool(flag) for flag in (args.release, args.conan, args.standard_test)) != 1:
        parser.error("exactly one of --release, --conan, --standard-test is required")
    if args.target_main and not args.standard_test:
        parser.error("--target-main only applies to --standard-test")
    if args.pull_request and not args.conan:
        parser.error("--pull-request only applies to --conan")

    generate_jobs_file(
        release=args.release,
        conan=args.conan,
        standard_test=args.standard_test,
        target_main=args.target_main,
        pull_request=args.pull_request,
    )


if __name__ == "__main__":
    main()
