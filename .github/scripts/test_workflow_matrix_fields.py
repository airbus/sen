# === test_workflow_matrix_fields.py ===================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Guard the matrix fields the workflows read.

A workflow step gated on `if: matrix.<field>` skips when that field does not exist, and a skipped
step reports success. So renaming a field in the matrix generator silently disables every step
gated on it, on every leg, with the job still green -- which is how the archive-completeness check
would stop running without anyone noticing.

This is the same shape of guard as test_main_yaml.py's ci-ok check, for the same reason: a name
that has to agree across two files and nothing making it agree.
"""

import dataclasses
import re
import sys
from pathlib import Path

WORKFLOWS = Path(__file__).resolve().parents[1] / "workflows"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_matrix_jobs import JobSpecification  # noqa: E402

# Set by GitHub or by the workflow itself rather than by the job specification.
NOT_FROM_THE_SPECIFICATION = frozenset({"lane", "sanitizer"})


def test_every_matrix_field_the_workflows_read_exists() -> None:
    """Every `matrix.<field>` in a workflow must be a field of the job specification."""
    known = {field.name for field in dataclasses.fields(JobSpecification)} | NOT_FROM_THE_SPECIFICATION
    referenced = set()
    for workflow in WORKFLOWS.glob("*.yaml"):
        referenced |= set(re.findall(r"matrix\.([a-z_]+)", workflow.read_text()))

    unknown = sorted(referenced - known)
    assert not unknown, (
        f"workflows read matrix fields that the job specification does not define: {unknown}. "
        "A step gated on one of these skips silently and reports success."
    )
