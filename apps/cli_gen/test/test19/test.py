# === test.py ==========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Encodes a cli_gen test to ensure code gen settings are correctly handled."""

import os
import subprocess
import sys


def main() -> None:
    """
    Encodes the setup and testing logic of running cli_gen.

    Integration test to check that the codegen settings work well in FOM code generation.
    """
    # input arguments
    exe_path = "./cli_gen"
    source_dir = os.path.dirname(os.path.abspath(__file__))
    output_file = os.path.join(os.getcwd(), "fom", "modulea-19.xml.h")
    target_word = "moduleAIntAcceptsSet"

    cmd = [
        exe_path,
        "cpp",
        "fom",
        f"--directories={source_dir}/fom",
        f"--settings={source_dir}/codegen_settings.json",
    ]

    print(f"Executing: {' '.join(cmd)}")

    result = subprocess.run(cmd, capture_output=True, text=True, check=False)

    if result.returncode != 0:
        print(f"Error: cli_gen failed with exit code {result.returncode}")
        print(f"Stdout: {result.stdout}")
        print(f"Stderr: {result.stderr}")
        sys.exit(1)

    if not os.path.exists(output_file):
        print(f"Error: Generated file NOT FOUND at {output_file}")
        sys.exit(1)

    with open(output_file, encoding="utf-8") as f:
        if target_word in f.read():
            # the test passes if the skeleton method for the checked property is present
            print(f"Success: Found '{target_word}' in {output_file}")
            sys.exit(0)
        else:
            print(f"Failure: '{target_word}' not found in the file.")
            sys.exit(1)


if __name__ == "__main__":
    main()
