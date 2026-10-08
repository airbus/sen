# === test_combined_report_job.py ======================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
"""Pins the job that renders one document over every configuration.

Each property below fails quietly rather than loudly. A gate limiting the job to main leaves a
pull request with no document to read, a dropped need renders a report one build short, and an
artifact named like a leg's is collected by the release as one more configuration.
"""

from pathlib import Path

import yaml

WORKFLOWS = Path(__file__).resolve().parents[1] / "workflows"
# What the release downloads per configuration, and what the document must not be mistaken for.
LEG_PREFIX = "test-report-"
REPORT_JOB = "combined-report"


def jobs_in(workflow: str) -> dict:
    """Returns the jobs of one workflow by name."""
    return yaml.safe_load((WORKFLOWS / workflow).read_text(encoding="utf-8"))["jobs"]


def artifacts_uploaded_by(job: dict) -> list[str]:
    """The names a job publishes, in the order its steps run."""
    return [
        step["with"]["name"]
        for step in job.get("steps", [])
        if "upload-artifact" in step.get("uses", "") and "name" in step.get("with", {})
    ]


def test_the_document_is_rendered_on_pull_requests_too():
    """No condition confines the job to main.

    The per-configuration render was removed when this job was added, so a gate here means a
    pull request produces no test document at all and the job's first run is on main, where a
    release is what waits for it.
    """
    condition = str(jobs_in("standard_test.yaml")[REPORT_JOB].get("if", ""))
    assert "building-main" not in condition, f"the job is gated: if {condition}"


def test_the_document_waits_for_every_leg():
    """It reads what the legs uploaded and checks the count against the matrix.

    Without the build dependency it would render whatever had arrived by then; without the
    matrix it would have nothing to compare that against.
    """
    needs = jobs_in("standard_test.yaml")[REPORT_JOB]["needs"]
    assert set(needs) >= {"build", "compute-matrix-jobs"}, needs


def test_the_document_is_not_named_like_a_configuration():
    """The release collects the legs by prefix and would read the document as one of them."""
    published = artifacts_uploaded_by(jobs_in("standard_test.yaml")[REPORT_JOB])
    assert published, "the job publishes nothing"
    for artifact in published:
        assert not artifact.startswith(LEG_PREFIX), artifact


def test_the_release_downloads_what_the_job_publishes():
    """The two names are written in different workflows and nothing else ties them together."""
    published = artifacts_uploaded_by(jobs_in("standard_test.yaml")[REPORT_JOB])
    wanted = [
        step["with"]["name"]
        for job in jobs_in("build_release.yaml").values()
        for step in job.get("steps", [])
        if "download-artifact" in step.get("uses", "") and "name" in step.get("with", {})
    ]
    assert set(published) & set(wanted), f"published {published}, the release asks for {wanted}"
