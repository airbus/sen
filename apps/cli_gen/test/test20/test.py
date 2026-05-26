# === test.py ==========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Encodes a cli_gen test to ensure FOM generations works correctly."""

import os
import subprocess
import sys


def main() -> None:
    """
    Encodes the setup and testing logic of running cli_gen.

    Integration test to check that the FOM generator creates Maybe types for optional parameters
    """
    # input arguments
    exe_path = "./cli_gen"
    source_dir = os.path.dirname(os.path.abspath(__file__))
    output_file = os.path.join(os.getcwd(), "fom", "modulea-20.xml.h")

    target_optional = "MaybeI32 myOptionalParam"
    target_required = "IntModuleA myRequiredParam"

    cmd = [exe_path, "cpp", "fom", f"--directories={source_dir}/fom"]

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
        content = f.read()

        if target_optional not in content:
            print(
                f"Failure: Optional parameter generation failed. '{target_optional}' "
                f"not found in the file {output_file}."
            )
            sys.exit(1)

        if target_required not in content:
            print(
                f"Failure: Required parameter generation failed. '{target_required}' "
                f"not found in the file {output_file}."
            )
            sys.exit(1)

        print(f"Success: Found correctly generated parameters in {output_file}")
        sys.exit(0)


if __name__ == "__main__":
    main()
