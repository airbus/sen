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
from dataclasses import dataclass, is_dataclass, asdict


def convert_dataclass_to_json(obj) -> dict:
    """Converts the given dataclass to json."""
    if is_dataclass(obj):
        return {
            key: convert_dataclass_to_json(value)
            for key, value in asdict(obj).items()
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
class JobSpecification:
    """Pipeline job specification that defines the configuration options."""
    name: str
    os: str
    compiler: Compiler
    std: tp.Literal[17, 20, 23]
    build_type: tp.Literal["Release", "Debug"]

    def as_json(self) -> dict:
        """Converts the job spec into json."""
        return convert_dataclass_to_json(self)


def compute_jobs() -> list[JobSpecification]:
    """Computes the list of pipeline jobs that should run."""
    jobs = []

    # Add gcc jobs
    jobs.append(
        JobSpecification("Basic GCC", "ubuntu-22.04",
                         Compiler("gcc", 12, "gcc-12", "g++-12"), 17, "Debug"))

    # Add clang jobs
    jobs.append(
        JobSpecification("Basic Clang", "ubuntu-24.04",
                         Compiler("clang", 20, "clang-20", "clang++-20"), 17,
                         "Debug"))

    return sorted(jobs)


def main() -> None:
    """Runs the job matrix generator."""
    jobs = compute_jobs()

    if output_file := os.environ.get("GITHUB_OUTPUT"):
        with open(output_file, "wt", encoding='utf-8') as matrix_file:
            matrix_file.write(
                f"jobs={json.dumps([j.as_json() for j in jobs])}")
    else:
        print("Error: No output file specified to write to.")


if __name__ == "__main__":
    main()
