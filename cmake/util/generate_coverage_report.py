#!/usr/bin/env python3

# === generate_coverage_report.py ======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

"""Script for generating llvm-cov based coverage reports."""

import typing as tp
import argparse
import glob
import os
import subprocess


def merge_coverage_profiles(llvm_profdata, profile_data_dir):
    """Merge separate coverage prof data into one merged profile with `llvm-profdata merge`"""
    print(" ├ Merging raw profiles...")

    raw_profiles = glob.glob(os.path.join(profile_data_dir, "*.profraw"))

    input_files = os.path.join(profile_data_dir, "profile_input_files")
    profdata_path = os.path.join(profile_data_dir, "merged_coverage.profdata")

    with open(input_files, "w") as manifest:
        manifest.write("\n".join(raw_profiles))

    cmd = [llvm_profdata, "merge"]
    cmd += ["-sparse"]
    cmd += ["-f", input_files]
    cmd += ["-o", profdata_path]

    subprocess.check_call(cmd)

    for raw_profile in raw_profiles:
        os.remove(raw_profile)
    os.remove(input_files)

    return profdata_path


def transform_to_binary_args(binaries) -> tp.List[str]:
    """Expands the binaries into a arg list that the llvm tools expect."""
    binary_cmd_line = [binaries[0]]
    for binary in binaries[1:]:
        binary_cmd_line.append("-object")
        binary_cmd_line.append(binary)

    return binary_cmd_line


def generate_html_report(llvm_cov, profile, report_dir, binaries, ignore_regex: tp.Optional[str]):
    """Generates a html report for the given coverage data."""
    print(" ├ Generating html report files...")

    cmd = [llvm_cov, "show"]
    cmd += transform_to_binary_args(binaries)
    cmd += ["-format", "html"]
    cmd += ["-instr-profile", profile]
    cmd += ["-o", report_dir]
    cmd += ["-show-line-counts-or-regions"]
    cmd += ["-show-directory-coverage"]
    cmd += ["-coverage-watermark=85,70"]
    cmd += ["-Xdemangler", "c++filt", "-Xdemangler", "-n"]
    if ignore_regex:
        cmd += [f"-ignore-filename-regex={ignore_regex}"]

    subprocess.check_call(cmd)


def generate_coverage_report(llvm_cov, profile, report_dir, binaries, ignore_regex: tp.Optional[str]):
    """Generates a coverage report for the given coverage data."""
    print(" ├ Generating coverage report...")

    cmd = [llvm_cov, "export"]
    cmd += transform_to_binary_args(binaries)
    cmd += ["-instr-profile", profile]
    cmd += ["-format", "lcov"]
    if ignore_regex:
        cmd += [f"-ignore-filename-regex={ignore_regex}"]

    with open(os.path.join(report_dir, "coverage.lcov"), "wb") as output_file:
        subprocess.check_call(
            cmd,
            stdout=output_file,
        )

def generate_coverage_summary(llvm_cov, profile, report_dir, binaries, ignore_regex: tp.Optional[str]):
    """Generates a coverage summary for the given coverage data."""
    print(" ├ Generating coverage summary...")

    cmd = [llvm_cov, "report"]
    cmd += transform_to_binary_args(binaries)
    cmd += ["-instr-profile", profile]
    if ignore_regex:
        cmd += [f"-ignore-filename-regex={ignore_regex}"]

    with open(os.path.join(report_dir, "coverage.txt"), "wb") as output_file:
        subprocess.check_call(
            cmd,
            stdout=output_file,
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("llvm_profdata", help="Path to the llvm-profdata binary")
    parser.add_argument("llvm_cov", help="Path to the llvm-cov binary")
    parser.add_argument("sen_coverage_data_dir")
    parser.add_argument("sen_coverage_report_output_dir")
    parser.add_argument("binaries", type=str, nargs="*")
    parser.add_argument("--ignore-filename-regex", type=str, default=None)
    args = parser.parse_args()

    if not len(args.binaries):
        print("No binaries specified! Cannot generated coverage report.")
        exit(1)

    print("Preparing coverage report")
    full_profile = merge_coverage_profiles(args.llvm_profdata, args.sen_coverage_data_dir)
    generate_html_report(
        args.llvm_cov,
        full_profile,
        args.sen_coverage_report_output_dir,
        args.binaries,
        args.ignore_filename_regex,
    )
    generate_coverage_report(
        args.llvm_cov,
        full_profile,
        args.sen_coverage_report_output_dir,
        args.binaries,
        args.ignore_filename_regex,
    )
    generate_coverage_summary(
        args.llvm_cov,
        full_profile,
        args.sen_coverage_report_output_dir,
        args.binaries,
        args.ignore_filename_regex,
    )
    print(" └ Generation finished")
